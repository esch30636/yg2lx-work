/*===========================================================================
 * test_cnn.c — Validation test for battery CNN inference engine
 *
 * Build (native):
 *   gcc -O2 -I../include -o test_cnn test_cnn.c ../src/cnn_inference.c -lm
 *
 * Build (cross):
 *   source /opt/yg2lx/environment-setup-aarch64-poky-linux
 *   $CC -O2 -I../include -o test_cnn test_cnn.c ../src/cnn_inference.c -lm
 *===========================================================================*/

#include "cnn_inference.h"
#include "model_meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* Sample IC curve: [2 channels × 128 timesteps] — replace with real data */
static void make_test_input(float *buf)
{
    /* Generate a synthetic IC curve for testing:
     * Channel 0: Gaussian peak centered at ~64 (simulated dV/dQ peak)
     * Channel 1: Linear ramp (simulated voltage)
     */
    for (int t = 0; t < 128; t++) {
        float x = (float)(t - 64) / 10.0f;
        buf[t] = expf(-0.5f * x * x) + 0.05f * ((float)rand() / RAND_MAX - 0.5f);
        buf[128 + t] = (float)t / 128.0f + 0.02f * ((float)rand() / RAND_MAX - 0.5f);
    }
}

static void print_result(const cnn_result_t *r)
{
    printf("\n══════════════════════════════════════\n");
    printf("  Battery SOH Inference Result\n");
    printf("══════════════════════════════════════\n\n");

    /* Stage classification */
    printf("  ── Aging Stage (softmax of logits) ──\n");
    float max_logit = r->stage_logits[0];
    int argmax = 0;
    float sum_exp = 0.0f;
    for (int i = 0; i < 3; i++) {
        if (r->stage_logits[i] > max_logit) {
            max_logit = r->stage_logits[i];
            argmax = i;
        }
        sum_exp += expf(r->stage_logits[i]);
    }

    const char *stage_names[] = {"Stage 0 (Healthy)", "Stage 1 (Degrading)", "Stage 2 (Critical)"};
    for (int i = 0; i < 3; i++) {
        float prob = expf(r->stage_logits[i]) / sum_exp;
        const char *marker = (i == argmax) ? " ◀══ PREDICTED" : "";
        printf("    %-25s  logit=%+8.4f  prob=%.4f%s\n",
               stage_names[i], r->stage_logits[i], prob, marker);
    }

    /* RUL */
    printf("\n  ── Remaining Useful Life ──\n");
    printf("    RUL (raw):  %.6f\n", r->rul);
    printf("\n══════════════════════════════════════\n");
}

int main(int argc, char **argv)
{
    float input[2 * 128];
    cnn_result_t result;

    /* Seed or use provided input */
    if (argc >= 2) {
        /* Read binary input file */
        FILE *fp = fopen(argv[1], "rb");
        if (!fp) {
            fprintf(stderr, "ERROR: Cannot open %s\n", argv[1]);
            return 1;
        }
        size_t n = fread(input, sizeof(float), 2 * 128, fp);
        if (n != 2 * 128) {
            fprintf(stderr, "WARNING: read %zu floats, expected 256\n", n);
        }
        fclose(fp);
        printf("Loaded input from: %s\n", argv[1]);
    } else {
        srand(42);
        make_test_input(input);
        printf("Using synthetic test input (seed=42)\n");
    }

    /* Init */
    printf("Initializing CNN inference engine...\n");
    cnn_ctx_t *ctx = cnn_init();
    if (!ctx) {
        fprintf(stderr, "ERROR: cnn_init() failed (out of memory?)\n");
        return 1;
    }
    printf("  OK.  Model weights: ~43K params (float32), buffers: %d floats\n", MAX_BUF_SIZE);

    /* Benchmark */
    int warmup = 3, iters = 100;
    for (int i = 0; i < warmup; i++)
        cnn_infer(ctx, input);

    clock_t start = clock();
    for (int i = 0; i < iters; i++)
        result = cnn_infer(ctx, input);
    clock_t end = clock();

    double ms_per = 1000.0 * (double)(end - start) / (double)CLOCKS_PER_SEC / (double)iters;

    print_result(&result);

    printf("\n  ── Performance (native build) ──\n");
    printf("    %d iterations, %.3f ms/inference\n", iters, ms_per);
    printf("    ~%.0f inferences/sec\n\n", 1000.0 / ms_per);

    cnn_free(ctx);
    return 0;
}
