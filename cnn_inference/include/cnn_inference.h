/*===========================================================================
 * cnn_inference.h — Battery SOH CNN Inference Engine (Pure C, Zero Deps)
 *
 * Model: 1D ResNet with GELU activations, dual output heads
 *   Input:  IC curve [batch=1, channels=2, timesteps=128] float32
 *   Output: stage_logits[3] — battery aging stage classification
 *           rul[1]         — Remaining Useful Life regression
 *
 * Target: MYD-YG2LX (Renesas RZ/G2L, Cortex-A55 ARMv8-A)
 *===========================================================================*/
#ifndef CNN_INFERENCE_H
#define CNN_INFERENCE_H

#include <stddef.h>

/* ── Public types ── */

/** Inference result */
typedef struct {
    float stage_logits[3];   /* logits for 3 aging stages (argmax = stage) */
    float rul;               /* Remaining Useful Life (normalized, model-specific) */
} cnn_result_t;

/** Opaque inference context */
typedef struct cnn_ctx cnn_ctx_t;

/* ── Public API ── */

/**
 * Allocate and initialize inference engine.
 * Loads all dequantized weights (linked via model_weights.h).
 * Returns NULL on allocation failure.
 */
cnn_ctx_t* cnn_init(void);

/**
 * Release all resources.
 */
void cnn_free(cnn_ctx_t *ctx);

/**
 * Run inference on a single IC curve sample.
 *
 * @param ctx   Initialized context from cnn_init()
 * @param input Float array of size [2 * 128], layout:
 *              input[0..127]   = channel 0 (typically dV/dQ or voltage)
 *              input[128..255] = channel 1 (typically current or temperature)
 * @return      Struct with .stage_logits[3] and .rul
 */
cnn_result_t cnn_infer(cnn_ctx_t *ctx, const float *input);

#endif /* CNN_INFERENCE_H */
