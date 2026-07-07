/*===========================================================================
 * verify_preprocess.cpp — CNN Preprocessing Byte-Identical Verification
 *
 * Compares battery_infer.c preprocessing (reference C implementation)
 * against CnnPreprocessor::process() (Qt/C++ HMI implementation).
 *
 * Compile (host):
 *   g++ -O2 -Wall -I../include -I../hmi/inference \
 *       -o verify_preprocess verify_preprocess.cpp ../hmi/inference/CnnPreprocessor.cpp -lm
 *
 * Usage:
 *   ./verify_preprocess [num_seeds=100]
 *
 * Exit code 0 = ALL TESTS PASSED (byte-identical for all seeds)
 * Exit code 1 = MISMATCH DETECTED
 *===========================================================================*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <math.h>   /* C math: isnan, isinf, fabsf in global namespace */

/* scalers.h constants (same as battery_infer.c includes) */
#include "scalers.h"

/* CnnPreprocessor from HMI */
#include "CnnPreprocessor.h"

/* ═══════════════════════════════════════════════════════════════════
 * Reference C implementation — copied verbatim from battery_infer.c
 * lines 78-105 (cmd_cnn function).
 *
 * This is the GOLDEN reference. CnnPreprocessor::process() MUST
 * produce identical output for identical raw_ic input.
 * ═══════════════════════════════════════════════════════════════════ */
static void reference_preprocess_c(const float raw_ic[128], float output[256])
{
    /* Step 1: IC normalization (lines 79-86) */
    for (int i = 0; i < 128; i++) {
        float val = raw_ic[i];
        if (isnan(val) || isinf(val)) val = 0.0f;
        val = (val - CNN_IC_SCALER_MEAN[i]) / CNN_IC_SCALER_STD[i];
        if (val > 5.0f) val = 5.0f;
        if (val < -5.0f) val = -5.0f;
        output[i] = val;
    }

    /* Step 2: Compute gradient along voltage axis (central difference, np.gradient)
     *   g[0]   = x[1] - x[0]               (forward at left edge)
     *   g[i]   = (x[i+1] - x[i-1]) * 0.5   (central, interior)
     *   g[127] = x[127] - x[126]           (backward at right edge)
     */
    output[128]     = output[1] - output[0];       /* edge: forward diff */
    output[255]     = output[127] - output[126];   /* edge: backward diff */
    float abs_max = fabsf(output[128]);
    if (fabsf(output[255]) > abs_max) abs_max = fabsf(output[255]);
    for (int i = 1; i < 127; i++) {
        float g = (output[i + 1] - output[i - 1]) * 0.5f;
        output[128 + i] = g;
        if (fabsf(g) > abs_max) abs_max = fabsf(g);
    }
    if (abs_max < 1e-6f) abs_max = 1.0f;

    /* Step 3: Normalize gradient (lines 99-105) */
    for (int i = 0; i < 128; i++) {
        float val = output[128 + i] / abs_max;
        val = (val - CNN_IG_SCALER_MEAN[i]) / CNN_IG_SCALER_STD[i];
        if (val > 5.0f) val = 5.0f;
        if (val < -5.0f) val = -5.0f;
        output[128 + i] = val;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Comparison helpers
 * ═══════════════════════════════════════════════════════════════════ */

struct CompareResult {
    bool identical;          /* memcmp == 0 */
    int first_mismatch_idx;  /* -1 if identical */
    float max_abs_diff;
    int max_abs_diff_idx;
    float max_rel_diff;
    int max_rel_diff_idx;
};

static CompareResult compare_outputs(const float ref[256], const float test[256])
{
    CompareResult r = {};
    r.identical = true;
    r.first_mismatch_idx = -1;
    r.max_abs_diff = 0.0f;
    r.max_abs_diff_idx = -1;
    r.max_rel_diff = 0.0f;
    r.max_rel_diff_idx = -1;

    for (int i = 0; i < 256; i++) {
        /* Check bit-identical via uint32_t */
        uint32_t ref_bits, test_bits;
        memcpy(&ref_bits, &ref[i], 4);
        memcpy(&test_bits, &test[i], 4);

        if (ref_bits != test_bits) {
            if (r.identical) {
                r.identical = false;
                r.first_mismatch_idx = i;
            }

            float abs_diff = fabsf(ref[i] - test[i]);
            if (abs_diff > r.max_abs_diff) {
                r.max_abs_diff = abs_diff;
                r.max_abs_diff_idx = i;
            }

            /* Relative difference (avoid div by zero) */
            float denom = fabsf(ref[i]);
            if (denom < 1e-12f) denom = 1e-12f;
            float rel_diff = abs_diff / denom;
            if (rel_diff > r.max_rel_diff) {
                r.max_rel_diff = rel_diff;
                r.max_rel_diff_idx = i;
            }
        }
    }
    return r;
}

/* ═══════════════════════════════════════════════════════════════════
 * Generate test data
 * ═══════════════════════════════════════════════════════════════════ */

/* Generate IC-like data: Gaussian peak + noise (matches DemoDataProvider) */
static void generate_ic_like(float raw_ic[128], unsigned int seed)
{
    srand(seed);
    /* Gaussian IC peak centered at ~64, height 2.8, with noise */
    for (int i = 0; i < 128; i++) {
        float x = (float)(i - 64) / 16.0f;  /* center at 64, sigma ~16 */
        float gaussian = 2.8f * expf(-x * x / 2.0f);
        float harmonic = 0.1f * sinf(4.0f * (float)i / 128.0f * 2.0f * 3.14159f);
        float noise = ((float)rand() / (float)RAND_MAX - 0.5f) * 0.06f;
        raw_ic[i] = gaussian + harmonic + noise;
    }
}

/* Generate completely random data */
static void generate_random(float raw_ic[128], unsigned int seed)
{
    srand(seed);
    for (int i = 0; i < 128; i++) {
        /* Mixture of normal range and extreme values */
        float r = (float)rand() / (float)RAND_MAX;
        if (r < 0.9f) {
            raw_ic[i] = (r - 0.5f) * 10.0f;  /* [-5, 5] typical */
        } else if (r < 0.95f) {
            raw_ic[i] = (r - 0.5f) * 2000.0f; /* large values (like real IC) */
        } else {
            raw_ic[i] = (r - 0.5f) * 1e-8f;   /* tiny values */
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════ */
int main(int argc, char **argv)
{
    int num_seeds = (argc > 1) ? atoi(argv[1]) : 100;
    if (num_seeds < 1) num_seeds = 1;

    printf("═══════════════════════════════════════════════════════════\n");
    printf("  CNN Preprocessing Byte-Identical Verification\n");
    printf("  Reference: battery_infer.c lines 78-105 (C)\n");
    printf("  Test:      CnnPreprocessor::process() (C++)\n");
    printf("  Seeds:     %d (0..%d)\n", num_seeds, num_seeds - 1);
    printf("═══════════════════════════════════════════════════════════\n\n");

    int total_tests = 0;
    int passed = 0;
    int failed = 0;
    float worst_abs = 0.0f, worst_rel = 0.0f;

    /* ── Test 1: IC-like data (simulates real battery data) ── */
    printf("── Test 1: IC-like Gaussian peak + noise ──\n");
    for (int seed = 0; seed < num_seeds; seed++) {
        float raw_ic[128];
        generate_ic_like(raw_ic, (unsigned int)(seed + 10000));

        float ref[256], test[256];
        reference_preprocess_c(raw_ic, ref);
        CnnPreprocessor::process(raw_ic, test);

        CompareResult cr = compare_outputs(ref, test);
        total_tests++;
        if (cr.identical) {
            passed++;
        } else {
            failed++;
            printf("  FAIL seed=%d: first mismatch at [%d] ref=%a test=%a abs=%.2e rel=%.2e\n",
                   seed, cr.first_mismatch_idx,
                   ref[cr.first_mismatch_idx], test[cr.first_mismatch_idx],
                   cr.max_abs_diff, cr.max_rel_diff);
            if (cr.max_abs_diff > worst_abs) worst_abs = cr.max_abs_diff;
            if (cr.max_rel_diff > worst_rel) worst_rel = cr.max_rel_diff;
        }
    }
    printf("  IC-like:   %d/%d passed", passed, num_seeds);
    if (failed > 0) printf("  ⚠ %d FAILED", failed);
    printf("\n\n");

    /* ── Test 2: Random data (wide range) ── */
    int prev_passed = passed;
    printf("── Test 2: Random data (wide range + extremes) ──\n");
    for (int seed = 0; seed < num_seeds; seed++) {
        float raw_ic[128];
        generate_random(raw_ic, (unsigned int)(seed + 20000));

        float ref[256], test[256];
        reference_preprocess_c(raw_ic, ref);
        CnnPreprocessor::process(raw_ic, test);

        CompareResult cr = compare_outputs(ref, test);
        total_tests++;
        if (cr.identical) {
            passed++;
        } else {
            failed++;
            printf("  FAIL seed=%d: first mismatch at [%d] ref=%a test=%a abs=%.2e rel=%.2e\n",
                   seed, cr.first_mismatch_idx,
                   ref[cr.first_mismatch_idx], test[cr.first_mismatch_idx],
                   cr.max_abs_diff, cr.max_rel_diff);
            if (cr.max_abs_diff > worst_abs) worst_abs = cr.max_abs_diff;
            if (cr.max_rel_diff > worst_rel) worst_rel = cr.max_rel_diff;
        }
    }
    printf("  Random:    %d/%d passed", passed - prev_passed, num_seeds);
    if (failed > 0) printf("  ⚠ %d FAILED", failed);
    printf("\n\n");

    /* ── Test 3: Edge cases ── */
    printf("── Test 3: Edge cases ──\n");

    /* 3a: All zeros */
    {
        float raw_ic[128] = {0.0f};
        float ref[256], test[256];
        reference_preprocess_c(raw_ic, ref);
        CnnPreprocessor::process(raw_ic, test);
        CompareResult cr = compare_outputs(ref, test);
        total_tests++;
        if (cr.identical) { passed++; printf("  All zeros: PASS\n"); }
        else { failed++; printf("  All zeros: FAIL first mismatch at [%d] abs=%.2e\n",
                                  cr.first_mismatch_idx, cr.max_abs_diff); }
    }

    /* 3b: All ones */
    {
        float raw_ic[128];
        for (int i = 0; i < 128; i++) raw_ic[i] = 1.0f;
        float ref[256], test[256];
        reference_preprocess_c(raw_ic, ref);
        CnnPreprocessor::process(raw_ic, test);
        CompareResult cr = compare_outputs(ref, test);
        total_tests++;
        if (cr.identical) { passed++; printf("  All ones:  PASS\n"); }
        else { failed++; printf("  All ones:  FAIL first mismatch at [%d] abs=%.2e\n",
                                  cr.first_mismatch_idx, cr.max_abs_diff); }
    }

    /* 3c: NaN/Inf contamination */
    {
        float raw_ic[128];
        for (int i = 0; i < 128; i++) raw_ic[i] = 1.0f;
        raw_ic[10] = NAN;
        raw_ic[50] = INFINITY;
        raw_ic[100] = -INFINITY;
        float ref[256], test[256];
        reference_preprocess_c(raw_ic, ref);
        CnnPreprocessor::process(raw_ic, test);
        CompareResult cr = compare_outputs(ref, test);
        total_tests++;
        if (cr.identical) { passed++; printf("  NaN/Inf:   PASS\n"); }
        else { failed++; printf("  NaN/Inf:   FAIL first mismatch at [%d] abs=%.2e\n",
                                  cr.first_mismatch_idx, cr.max_abs_diff); }
    }

    /* 3d: Very large values (close to real battery IC magnitude) */
    {
        float raw_ic[128];
        for (int i = 0; i < 128; i++) raw_ic[i] = (i % 2 == 0) ? 2000.0f : -2000.0f;
        float ref[256], test[256];
        reference_preprocess_c(raw_ic, ref);
        CnnPreprocessor::process(raw_ic, test);
        CompareResult cr = compare_outputs(ref, test);
        total_tests++;
        if (cr.identical) { passed++; printf("  Large val: PASS\n"); }
        else { failed++; printf("  Large val: FAIL first mismatch at [%d] abs=%.2e\n",
                                  cr.first_mismatch_idx, cr.max_abs_diff); }
    }

    /* 3e: Subnormal float values */
    {
        float raw_ic[128];
        for (int i = 0; i < 128; i++) raw_ic[i] = 1e-40f;
        float ref[256], test[256];
        reference_preprocess_c(raw_ic, ref);
        CnnPreprocessor::process(raw_ic, test);
        CompareResult cr = compare_outputs(ref, test);
        total_tests++;
        if (cr.identical) { passed++; printf("  Subnormal: PASS\n"); }
        else { failed++; printf("  Subnormal: FAIL first mismatch at [%d] abs=%.2e\n",
                                  cr.first_mismatch_idx, cr.max_abs_diff); }
    }

    printf("\n");

    /* ── Test 4: Single input full dump (seed=42, for manual inspection) ── */
    printf("── Test 4: Full dump for seed=42 (IC-like) ──\n");
    {
        float raw_ic[128];
        generate_ic_like(raw_ic, 42);
        float ref[256], test[256];
        reference_preprocess_c(raw_ic, ref);
        CnnPreprocessor::process(raw_ic, test);
        CompareResult cr = compare_outputs(ref, test);

        printf("  Identical: %s\n", cr.identical ? "YES ✅" : "NO ❌");
        if (!cr.identical) {
            printf("  First mismatch: [%d] ref=%.9g test=%.9g diff=%.2e\n",
                   cr.first_mismatch_idx,
                   ref[cr.first_mismatch_idx], test[cr.first_mismatch_idx],
                   fabsf(ref[cr.first_mismatch_idx] - test[cr.first_mismatch_idx]));
        }
        printf("  Max abs diff: %.6e at [%d]\n", cr.max_abs_diff, cr.max_abs_diff_idx);
        printf("  Max rel diff: %.6e at [%d]\n", cr.max_rel_diff, cr.max_rel_diff_idx);

        /* Print first 8 elements of each channel for spot-check */
        printf("  Channel 0 (IC) first 8:\n");
        printf("    ref:  ");
        for (int i = 0; i < 8; i++) printf("%11.8f ", ref[i]);
        printf("\n    test: ");
        for (int i = 0; i < 8; i++) printf("%11.8f ", test[i]);
        printf("\n  Channel 1 (gradient) first 8:\n");
        printf("    ref:  ");
        for (int i = 0; i < 8; i++) printf("%11.8f ", ref[128 + i]);
        printf("\n    test: ");
        for (int i = 0; i < 8; i++) printf("%11.8f ", test[128 + i]);
        printf("\n");
    }

    /* ═══════════════════════════════════════════════════════════════
     * Summary
     * ═══════════════════════════════════════════════════════════════ */
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("  SUMMARY\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Total tests: %d\n", total_tests);
    printf("  Passed:      %d\n", passed);
    printf("  Failed:      %d\n", failed);

    if (failed == 0) {
        printf("\n  ✅ ALL TESTS PASSED — preprocessing is byte-identical.\n");
        printf("  CnnPreprocessor::process() == battery_infer.c preprocessing\n");
        printf("\n");
        return 0;
    } else {
        printf("\n  ❌ %d TEST(S) FAILED\n", failed);
        printf("  Worst absolute difference: %.6e\n", worst_abs);
        printf("  Worst relative difference: %.6e\n", worst_rel);
        if (worst_rel < 1e-5f) {
            printf("\n  Differences are below 1e-5 relative — likely FP rounding,\n");
            printf("  not a logic bug. Acceptable for float32 inference.\n");
        }
        printf("\n");
        return 1;
    }
}
