/*===========================================================================
 * battery_inference.h — Dual-Model Battery SOH Inference Engine
 *
 * Pure C, zero dependencies. Targets MYD-YG2LX (RZ/G2L Cortex-A55).
 *
 * Models:
 *   PINN — Physics-Informed Neural Network (fast screening)
 *          132-d features → SOH ∈ [0, 1]
 *   CNN  — Residual 1D-CNN (precise assessment)
 *          IC curve (2,128) → 3-stage + RUL
 *
 * Usage:
 *   #include "battery_inference.h"
 *
 *   // PINN: fast SOH screening
 *   pinn_ctx_t *pinn = pinn_init();
 *   float soh = pinn_predict(pinn, features_132);
 *   pinn_free(pinn);
 *
 *   // CNN: stage classification + RUL
 *   cnn_ctx_t *cnn = cnn_init();
 *   cnn_result_t r = cnn_predict(cnn, ic_curve_256);
 *   // r.stage ∈ {0,1,2}, r.rul ∈ [0,1]
 *   cnn_free(cnn);
 *===========================================================================*/
#ifndef BATTERY_INFERENCE_H
#define BATTERY_INFERENCE_H

#include <stddef.h>

/* ── CNN result ── */
typedef struct {
    float stage_logits[3];   /* raw logits; argmax → stage */
    float rul;               /* remaining useful life ∈ [0, 1] */
} cnn_result_t;

/* ── Opaque contexts ── */
typedef struct pinn_ctx pinn_ctx_t;
typedef struct cnn_ctx  cnn_ctx_t;

/* ═══════════════════════════════════════════════════════════════════
 * PINN API — Fast SOH screening
 * ═══════════════════════════════════════════════════════════════════ */

pinn_ctx_t* pinn_init(void);
void        pinn_free(pinn_ctx_t *ctx);

/**
 * Predict SOH from raw 132-d feature vector.
 * @param features  [132] float32, raw (un-normalized) features:
 *                  [0..127]   IC curve on voltage grid
 *                  [128]      temperature
 *                  [129]      log(cycle_count)
 *                  [130]      dV proxy (start voltage drop)
 *                  [131]      measured capacity
 * @return SOH ∈ [0, 1]
 */
float pinn_predict(pinn_ctx_t *ctx, const float *features);

/* ═══════════════════════════════════════════════════════════════════
 * CNN API — Precise stage + RUL assessment
 * ═══════════════════════════════════════════════════════════════════ */

cnn_ctx_t*   cnn_init(void);
void         cnn_free(cnn_ctx_t *ctx);

/**
 * Predict aging stage and RUL from IC curve.
 * @param input  [256] float32, dual-channel IC curve:
 *               [0..127]   channel 0: IC curve (dQ/dV)
 *               [128..255] channel 1: IC gradient (computed by caller)
 * @return stage_logits[3] + RUL
 */
cnn_result_t cnn_predict(cnn_ctx_t *ctx, const float *input);

#endif /* BATTERY_INFERENCE_H */
