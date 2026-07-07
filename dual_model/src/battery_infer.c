/*===========================================================================
 * battery_infer.c — Dual-Model Battery SOH Inference CLI
 *
 * Usage:
 *   battery_infer pinn [input_132d.bin]    PINN: 132-d features → SOH
 *   battery_infer cnn  [ic_curve_128.bin]  CNN: 128-point IC curve → stage + RUL
 *   battery_infer benchmark                 Run both models with timing
 *===========================================================================*/
#include "battery_inference.h"
#include "scalers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

static const char *STAGE_NAMES[] = {"healthy", "degrading", "EOL"};

/* ── Load float32 binary file ── */
static int load_f32(const char *path, float *buf, int expected)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "ERROR: cannot open '%s'\n", path);
        return -1;
    }
    size_t n = fread(buf, sizeof(float), expected, fp);
    fclose(fp);
    if (n != (size_t)expected) {
        fprintf(stderr, "ERROR: expected %d floats, got %zu\n", expected, n);
        return -1;
    }
    return 0;
}

/* ── PINN inference ── */
static int cmd_pinn(const char *path)
{
    pinn_ctx_t *ctx = pinn_init();
    if (!ctx) { fprintf(stderr, "ERROR: pinn_init failed\n"); return 1; }

    float features[132];
    if (path) {
        if (load_f32(path, features, 132)) { pinn_free(ctx); return 1; }
    } else {
        /* Synthetic random data for testing */
        srand(42);
        for (int i = 0; i < 132; i++)
            features[i] = (float)rand() / (float)RAND_MAX;
        printf("(using synthetic data, seed=42)\n");
    }

    float soh = pinn_predict(ctx, features);
    printf("PINN SOH: %.4f  (%.1f%%)\n", soh, soh * 100.0f);

    pinn_free(ctx);
    return 0;
}

/* ── CNN inference ── */
static int cmd_cnn(const char *path)
{
    cnn_ctx_t *ctx = cnn_init();
    if (!ctx) { fprintf(stderr, "ERROR: cnn_init failed\n"); return 1; }

    float input[256];
    if (path) {
        /* Load raw IC curve (128 floats) and apply full preprocessing pipeline
         * matching ONNX Runtime expectations:
         *   1. Normalize IC with ic_scaler, clip [-5,5]
         *   2. Compute IC gradient
         *   3. Normalize gradient (abs_max), apply ig_scaler, clip [-5,5]
         *   4. Stack as dual-channel (2, 128)
         */
        float raw_ic[128];
        if (load_f32(path, raw_ic, 128)) { cnn_free(ctx); return 1; }

        /* Step 1: IC normalization */
        for (int i = 0; i < 128; i++) {
            float val = raw_ic[i];
            if (isnan(val) || isinf(val)) val = 0.0f;
            val = (val - CNN_IC_SCALER_MEAN[i]) / CNN_IC_SCALER_STD[i];
            if (val > 5.0f) val = 5.0f;
            if (val < -5.0f) val = -5.0f;
            input[i] = val;
        }

        /* Step 2: Compute gradient along voltage axis
         * Central difference matching np.gradient (edge_order=1):
         *   g[0]   = x[1] - x[0]               (forward at left edge)
         *   g[i]   = (x[i+1] - x[i-1]) / 2     (central, interior)
         *   g[127] = x[127] - x[126]           (backward at right edge)
         */
        input[128]     = input[1] - input[0];       /* edge: forward diff */
        input[255]     = input[127] - input[126];   /* edge: backward diff */
        float abs_max = fabsf(input[128]);
        if (fabsf(input[255]) > abs_max) abs_max = fabsf(input[255]);
        for (int i = 1; i < 127; i++) {
            float g = (input[i + 1] - input[i - 1]) * 0.5f;
            input[128 + i] = g;
            if (fabsf(g) > abs_max) abs_max = fabsf(g);
        }
        if (abs_max < 1e-6f) abs_max = 1.0f;

        /* Step 3: Normalize gradient */
        for (int i = 0; i < 128; i++) {
            float val = input[128 + i] / abs_max;
            val = (val - CNN_IG_SCALER_MEAN[i]) / CNN_IG_SCALER_STD[i];
            if (val > 5.0f) val = 5.0f;
            if (val < -5.0f) val = -5.0f;
            input[128 + i] = val;
        }
    } else {
        /* Synthetic random data (raw, pre-normalized — just for testing) */
        srand(42);
        for (int i = 0; i < 256; i++)
            input[i] = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
        printf("(using synthetic data, seed=42)\n");
    }

    cnn_result_t r = cnn_predict(ctx, input);
    int stage = 0;
    if (r.stage_logits[1] > r.stage_logits[stage]) stage = 1;
    if (r.stage_logits[2] > r.stage_logits[stage]) stage = 2;

    printf("CNN Stage: %s (%d)  |  logits: [%.4f, %.4f, %.4f]\n",
           STAGE_NAMES[stage], stage,
           r.stage_logits[0], r.stage_logits[1], r.stage_logits[2]);
    printf("CNN RUL:   %.4f\n", r.rul);

    cnn_free(ctx);
    return 0;
}

/* ── Benchmark ── */
static int cmd_benchmark(void)
{
    struct timespec t0, t1;
    float features[132], ic_input[256];

    /* Generate consistent test data */
    srand(42);
    for (int i = 0; i < 132; i++) features[i] = (float)rand() / (float)RAND_MAX;
    for (int i = 0; i < 256; i++) ic_input[i] = (float)rand() / (float)RAND_MAX;

    printf("Warming up...\n");

    pinn_ctx_t *pinn = pinn_init();
    cnn_ctx_t  *cnn  = cnn_init();
    if (!pinn || !cnn) { fprintf(stderr, "init failed\n"); return 1; }

    /* Warmup */
    for (int i = 0; i < 10; i++) {
        pinn_predict(pinn, features);
        cnn_predict(cnn, ic_input);
    }

    /* PINN benchmark */
    int n = 1000;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < n; i++)
        pinn_predict(pinn, features);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double pinn_ms = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / (double)n / 1e6;

    /* CNN benchmark */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < n; i++)
        cnn_predict(cnn, ic_input);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double cnn_ms = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / (double)n / 1e6;

    /* Single-shot inference to show output */
    float soh = pinn_predict(pinn, features);
    cnn_result_t r = cnn_predict(cnn, ic_input);
    int stage = 0;
    if (r.stage_logits[1] > r.stage_logits[stage]) stage = 1;
    if (r.stage_logits[2] > r.stage_logits[stage]) stage = 2;

    printf("\n═══════════════════════════════════════════════\n");
    printf("  Benchmark Results (%d runs each)\n", n);
    printf("═══════════════════════════════════════════════\n");
    printf("  PINN:  %8.2f ms  SOH = %.4f (%.1f%%)\n", pinn_ms, soh, soh * 100.0f);
    printf("  CNN:   %8.2f ms  Stage: %s, RUL = %.4f\n", cnn_ms, STAGE_NAMES[stage], r.rul);
    printf("  Total: %8.2f ms  (%.0f inferences/sec)\n",
           pinn_ms + cnn_ms, 1000.0 / (pinn_ms + cnn_ms));
    printf("═══════════════════════════════════════════════\n");

    pinn_free(pinn);
    cnn_free(cnn);
    return 0;
}

/* ── Main ── */
int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: battery_infer <pinn|cnn|benchmark> [input.bin]\n");
        printf("\n");
        printf("  battery_infer pinn  [features_132d.bin]   PINN SOH regression\n");
        printf("  battery_infer cnn   [ic_curve_128.bin]    CNN stage + RUL\n");
        printf("  battery_infer benchmark                    Run both with timing\n");
        return 1;
    }

    if (!strcmp(argv[1], "pinn"))
        return cmd_pinn(argc > 2 ? argv[2] : NULL);
    else if (!strcmp(argv[1], "cnn"))
        return cmd_cnn(argc > 2 ? argv[2] : NULL);
    else if (!strcmp(argv[1], "benchmark"))
        return cmd_benchmark();
    else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        return 1;
    }
}
