/*===========================================================================
 * pinn_inference.c — PINN Battery SOH Inference (Pure C)
 *
 * Architecture:
 *   Linear(132→128) → LayerNorm → GELU
 *   → Linear(128→128) → LayerNorm → GELU
 *   → Linear(128→64)
 *   → ResidualBlock: FC(64→64)+Norm+GELU → FC(64→64)+Norm → +skip → GELU
 *   → FC(64→32)+Norm+GELU → FC(32→1) → Sigmoid → SOH
 *
 * All weights pre-dequantized to float32 (via export_weights.py).
 *===========================================================================*/

#include "battery_inference.h"
#include "pinn_weights.h"
#include "pinn_meta.h"
#include "scalers.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════
 * Internal context
 * ═══════════════════════════════════════════════════════════════════ */
struct pinn_ctx {
    float *buf_a;    /* primary scratch */
    float *buf_b;    /* secondary scratch */
    float *buf_c;    /* residual/skip save */
};

/* ═══════════════════════════════════════════════════════════════════
 * Math helpers
 * ═══════════════════════════════════════════════════════════════════ */

/** GELU: x * 0.5 * (1 + erf(x / sqrt(2))) */
static inline float gelu(float x) {
    return x * 0.5f * (1.0f + erff(x * 0.7071067811865475f));
}

static void gelu_inplace(float *x, int n) {
    for (int i = 0; i < n; i++) x[i] = gelu(x[i]);
}

/** Sigmoid: 1 / (1 + exp(-x)) */
static inline float sigmoid(float x) {
    if (x >= 0) return 1.0f / (1.0f + expf(-x));
    else { float ex = expf(x); return ex / (1.0f + ex); }
}

/* ═══════════════════════════════════════════════════════════════════
 * Linear (Fully Connected) layer
 *
 * ONNX Gemm: Y = A @ B + C
 *   A: (batch, in_dim)  input
 *   B: (in_dim, out_dim) weight
 *   C: (out_dim,)        bias
 *
 * Memory layout: weight[i * out_dim + o]
 * ═══════════════════════════════════════════════════════════════════ */
static void linear(const float *x, int in_dim, int out_dim,
                   const float *weight, const float *bias,
                   float *out)
{
    for (int o = 0; o < out_dim; o++) {
        float sum = bias ? bias[o] : 0.0f;
        for (int i = 0; i < in_dim; i++)
            sum += x[i] * weight[i * out_dim + o];
        out[o] = sum;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * LayerNorm: y = (x - mean) / sqrt(var + eps) * gamma + beta
 * ═══════════════════════════════════════════════════════════════════ */
static void layernorm(const float *x, int dim,
                      const float *gamma, const float *beta,
                      float *out)
{
    /* Compute mean */
    float mean = 0.0f;
    for (int i = 0; i < dim; i++) mean += x[i];
    mean /= (float)dim;

    /* Compute variance */
    float var = 0.0f;
    for (int i = 0; i < dim; i++) {
        float d = x[i] - mean;
        var += d * d;
    }
    var /= (float)dim;

    /* Normalize + scale + shift */
    float inv_std = 1.0f / sqrtf(var + LAYERNORM_EPS);
    for (int i = 0; i < dim; i++) {
        float x_hat = (x[i] - mean) * inv_std;
        float g = gamma ? gamma[i] : 1.0f;
        float b = beta  ? beta[i]  : 0.0f;
        out[i] = x_hat * g + b;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Single Linear + LayerNorm + GELU block (used 3× in encoder)
 * ═══════════════════════════════════════════════════════════════════ */
static void linear_norm_gelu(const float *x,
                             int in_dim, int out_dim,
                             const float *weight, const float *bias,
                             const float *gn, const float *bn,
                             float *out, float *tmp)
{
    linear(x, in_dim, out_dim, weight, bias, tmp);
    layernorm(tmp, out_dim, gn, bn, out);   /* out = LN(tmp) */
    gelu_inplace(out, out_dim);              /* out = GELU(out) */
}

/* ═══════════════════════════════════════════════════════════════════
 * Residual block:
 *   skip = x
 *   x = fc1(x) → norm1 → GELU
 *   x = fc2(x) → norm2
 *   x = GELU(x + skip)
 * ═══════════════════════════════════════════════════════════════════ */
static void residual_block(pinn_ctx_t *ctx)
{
    int dim = RES_FC1_IN;   /* 64 */
    float *skip = ctx->buf_b;   /* save original input */
    float *tmp  = ctx->buf_c;   /* scratch for intermediate (MUST NOT alias buf_a!) */

    /* Keep a copy for skip */
    memcpy(skip, ctx->buf_a, dim * sizeof(float));

    /* fc1 → norm1 → GELU: buf_a(input) → tmp → buf_a */
    linear(ctx->buf_a, dim, dim,
           res_block_fc1_weight, res_block_fc1_bias, tmp);
    layernorm(tmp, dim, res_block_norm1_weight, res_block_norm1_bias, ctx->buf_a);
    gelu_inplace(ctx->buf_a, dim);

    /* fc2 → norm2: buf_a → tmp → buf_a */
    linear(ctx->buf_a, dim, dim,
           res_block_fc2_weight, res_block_fc2_bias, tmp);
    layernorm(tmp, dim, res_block_norm2_weight, res_block_norm2_bias, ctx->buf_a);

    /* GELU(skip + fc2_norm_out) → buf_a */
    for (int i = 0; i < dim; i++)
        ctx->buf_a[i] = gelu(skip[i] + ctx->buf_a[i]);
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

pinn_ctx_t* pinn_init(void)
{
    pinn_ctx_t *ctx = calloc(1, sizeof(pinn_ctx_t));
    if (!ctx) return NULL;

    /* Allocate buffers (max dim = 128 for encoder layers) */
    ctx->buf_a = calloc(PINN_MAX_BUF_SIZE, sizeof(float));
    ctx->buf_b = calloc(PINN_MAX_BUF_SIZE, sizeof(float));
    ctx->buf_c = calloc(PINN_MAX_BUF_SIZE, sizeof(float));

    if (!ctx->buf_a || !ctx->buf_b || !ctx->buf_c) {
        pinn_free(ctx);
        return NULL;
    }
    return ctx;
}

void pinn_free(pinn_ctx_t *ctx)
{
    if (ctx) {
        free(ctx->buf_a);
        free(ctx->buf_b);
        free(ctx->buf_c);
        free(ctx);
    }
}

float pinn_predict(pinn_ctx_t *ctx, const float *features)
{
    float *buf = ctx->buf_a;     /* primary working buffer */
    float *tmp = ctx->buf_b;     /* secondary scratch */

    /* ═══════════════════════════════════════════════════════════
     * 1. Input normalization: StandardScaler → clip[-5, 5]
     * ═══════════════════════════════════════════════════════════ */
    for (int i = 0; i < PINN_INPUT_DIM; i++) {
        float val = features[i];
        /* Handle NaN/Inf */
        if (isnan(val) || isinf(val)) val = 0.0f;
        val = (val - PINN_SCALER_MEAN[i]) / PINN_SCALER_STD[i];
        if (val > 5.0f) val = 5.0f;
        if (val < -5.0f) val = -5.0f;
        buf[i] = val;
    }

    /* ═══════════════════════════════════════════════════════════
     * 2. Encoder: 3× Linear+LayerNorm+GELU (but last one no Norm/GELU)
     * ═══════════════════════════════════════════════════════════ */

    /* Encoder block 0: Linear(132→128) + Norm + GELU
     *   linear_norm_gelu does: linear(x) → tmp, layernorm(tmp) → out, gelu(out)
     *   So: out=buf (final output), tmp=tmp (scratch) */
    linear_norm_gelu(buf,
                     ENC_FC1_IN, ENC_FC1_OUT,
                     encoder_0_weight, encoder_0_bias,
                     encoder_1_weight, encoder_1_bias,
                     buf, tmp);

    /* Encoder block 1: Linear(128→128) + Norm + GELU */
    linear_norm_gelu(buf,
                     ENC_FC2_IN, ENC_FC2_OUT,
                     encoder_4_weight, encoder_4_bias,
                     encoder_5_weight, encoder_5_bias,
                     buf, tmp);

    /* Encoder block 2: Linear(128→64) — NO Norm, NO GELU (as per ONNX) */
    linear(buf, ENC_FC3_IN, ENC_FC3_OUT,
           encoder_8_weight, encoder_8_bias, tmp);
    /* Copy to buf_a for residual block */
    memcpy(buf, tmp, ENC_FC3_OUT * sizeof(float));

    /* ═══════════════════════════════════════════════════════════
     * 3. Residual block
     * ═══════════════════════════════════════════════════════════ */
    residual_block(ctx);
    /* Result now in ctx->buf_a[0..63] */

    /* ═══════════════════════════════════════════════════════════
     * 4. Head: Linear(64→32) + Norm + GELU → Linear(32→1) → Sigmoid
     * ═══════════════════════════════════════════════════════════ */
    linear(ctx->buf_a, HEAD_FC1_IN, HEAD_FC1_OUT,
           head_0_weight, head_0_bias, tmp);
    layernorm(tmp, HEAD_NORM_DIM, head_1_weight, head_1_bias, buf);
    gelu_inplace(buf, HEAD_NORM_DIM);

    float logit;
    linear(buf, HEAD_FC2_IN, HEAD_FC2_OUT,
           head_3_weight, head_3_bias, &logit);

    return sigmoid(logit);
}
