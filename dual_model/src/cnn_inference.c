/*===========================================================================
 * cnn_inference.c — Battery SOH CNN Inference Engine (Pure C)
 *
 * Architecture (from PINN_CNN):
 *   Input [1, 2, 128] — IC curve + IC gradient dual-channel
 *   → Stem:  Conv1D(2→16, k7, s2, p3) + GELU         → [16, 64]
 *   → Body0: Conv(16→16,k7)+GELU + Conv(16→16,k7)+GELU + identity skip
 *            + MaxPool(k2)                             → [16, 32]
 *   → Body1: Conv(16→32,k7)+GELU + Conv(32→32,k7)+GELU + 1×1 SC
 *            + MaxPool(k2)                             → [32, 16]
 *   → Body2: Conv(32→48,k5)+GELU + Conv(48→48,k5)+GELU + 1×1 SC
 *            + MaxPool(k2)                             → [48, 8]
 *   → GAP → [48] features
 *   → cls_head: FC(48→48)+GELU → FC(48→24)+GELU → FC(24→3)
 *   → rul_head: FC(48→48)+GELU → FC(48→24)+GELU → FC(24→1)
 *
 * All weights pre-dequantized to float32 (via export_weights.py).
 *===========================================================================*/

#include "battery_inference.h"
#include "cnn_weights.h"
#include "cnn_meta.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════
 * Internal context
 * ═══════════════════════════════════════════════════════════════════ */
struct cnn_ctx {
    float *buf_a;
    float *buf_b;
    float *buf_c;
};

/* ═══════════════════════════════════════════════════════════════════
 * Math helpers
 * ═══════════════════════════════════════════════════════════════════ */

static inline float gelu(float x) {
    return x * 0.5f * (1.0f + erff(x * 0.7071067811865475f));
}

static void gelu_inplace(float *x, int n) {
    for (int i = 0; i < n; i++) x[i] = gelu(x[i]);
}

/* ═══════════════════════════════════════════════════════════════════
 * 1D Convolution — NCW layout
 *
 * Weight layout: [out_ch][in_ch][kernel] flat
 *   stored as: w[((oc * in_ch) + ic) * ks + k]
 * Input:  [ch][len] flat as x[ch * len + t]
 * Output: [out_ch][out_len] flat as out[oc * out_len + t]
 * ═══════════════════════════════════════════════════════════════════ */
static void conv1d(const float *x, int in_len, int in_ch, int out_ch,
                   int kernel, int stride, int pad,
                   const float *weight, const float *bias,
                   float *out)
{
    int out_len = (in_len - kernel + 2 * pad) / stride + 1;

    for (int oc = 0; oc < out_ch; oc++) {
        for (int t = 0; t < out_len; t++) {
            float sum = bias ? bias[oc] : 0.0f;
            for (int ic = 0; ic < in_ch; ic++) {
                for (int k = 0; k < kernel; k++) {
                    int pos = t * stride + k - pad;
                    if (pos >= 0 && pos < in_len) {
                        int w_idx = ((oc * in_ch) + ic) * kernel + k;
                        sum += x[ic * in_len + pos] * weight[w_idx];
                    }
                }
            }
            out[oc * out_len + t] = sum;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * MaxPool1D
 * ═══════════════════════════════════════════════════════════════════ */
static void maxpool1d(const float *x, int in_len, int ch,
                      int kernel, int stride, float *out)
{
    int out_len = (in_len - kernel) / stride + 1;
    for (int c = 0; c < ch; c++) {
        for (int t = 0; t < out_len; t++) {
            float max_val = -INFINITY;
            for (int k = 0; k < kernel; k++)
                if (x[c * in_len + t * stride + k] > max_val)
                    max_val = x[c * in_len + t * stride + k];
            out[c * out_len + t] = max_val;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * GlobalAveragePool1D
 * ═══════════════════════════════════════════════════════════════════ */
static void global_avg_pool1d(const float *x, int len, int ch, float *out)
{
    for (int c = 0; c < ch; c++) {
        float sum = 0.0f;
        const float *row = x + c * len;
        for (int t = 0; t < len; t++) sum += row[t];
        out[c] = sum / (float)len;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Linear (ONNX Gemm: Y = A @ B + C, B layout [in][out])
 * ═══════════════════════════════════════════════════════════════════ */
static void linear(const float *x, int in_dim, int out_dim,
                   const float *weight, const float *bias, float *out)
{
    for (int o = 0; o < out_dim; o++) {
        float sum = bias ? bias[o] : 0.0f;
        for (int i = 0; i < in_dim; i++)
            sum += x[i] * weight[i * out_dim + o];
        out[o] = sum;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Body Block with optional channel-expansion shortcut.
 *
 * Matches PyTorch ResBlock:
 *   residual = x  (or 1×1 conv if channels change)
 *   out = conv1(x)  → GELU
 *   out = conv2(out)
 *   out += residual
 *   out = GELU(out)
 *
 * CRITICAL: GELU is applied AFTER the residual add, not before.
 * conv1 writes to buf_b (NOT buf_a) to avoid in-place aliasing.
 * ═══════════════════════════════════════════════════════════════════ */
static void body_block(cnn_ctx_t *ctx,
                       const float *block_input,
                       int in_ch, int out_ch,
                       int in_len,
                       int conv1_kernel, int conv1_pad,
                       int conv2_kernel, int conv2_pad,
                       const float *conv1_w, const float *conv1_b,
                       const float *conv2_w, const float *conv2_b,
                       const float *sc_w,    const float *sc_b)
{
    int conv1_out_len = (in_len - conv1_kernel + 2 * conv1_pad) / 1 + 1;
    int conv2_out_len = (conv1_out_len - conv2_kernel + 2 * conv2_pad) / 1 + 1;

    /* Save input for skip connection */
    int in_size = in_ch * in_len;
    memcpy(ctx->buf_c, block_input, in_size * sizeof(float));

    /* Conv1 → buf_b */
    conv1d(block_input, in_len, in_ch, out_ch,
           conv1_kernel, 1, conv1_pad,
           conv1_w, conv1_b, ctx->buf_b);
    gelu_inplace(ctx->buf_b, out_ch * conv1_out_len);

    /* Conv2: buf_b → buf_a (no GELU yet — applied after skip add) */
    conv1d(ctx->buf_b, conv1_out_len, out_ch, out_ch,
           conv2_kernel, 1, conv2_pad,
           conv2_w, conv2_b, ctx->buf_a);

    /* Residual add + GELU → buf_b */
    if (in_ch == out_ch) {
        /* Identity shortcut: buf_b = GELU(buf_a + buf_c) */
        for (int i = 0; i < out_ch * conv2_out_len; i++)
            ctx->buf_b[i] = ctx->buf_a[i] + ctx->buf_c[i];
        gelu_inplace(ctx->buf_b, out_ch * conv2_out_len);
    } else {
        /* 1×1 conv shortcut: buf_c → buf_b, then add + GELU */
        conv1d(ctx->buf_c, in_len, in_ch, out_ch,
               1, 1, 0, sc_w, sc_b, ctx->buf_b);
        for (int i = 0; i < out_ch * conv2_out_len; i++)
            ctx->buf_b[i] += ctx->buf_a[i];
        gelu_inplace(ctx->buf_b, out_ch * conv2_out_len);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════ */

cnn_ctx_t* cnn_init(void)
{
    cnn_ctx_t *ctx = calloc(1, sizeof(cnn_ctx_t));
    if (!ctx) return NULL;

    ctx->buf_a = calloc(CNN_MAX_BUF_SIZE, sizeof(float));
    ctx->buf_b = calloc(CNN_MAX_BUF_SIZE, sizeof(float));
    ctx->buf_c = calloc(CNN_MAX_BUF_SIZE, sizeof(float));

    if (!ctx->buf_a || !ctx->buf_b || !ctx->buf_c) {
        cnn_free(ctx);
        return NULL;
    }
    return ctx;
}

void cnn_free(cnn_ctx_t *ctx)
{
    if (ctx) {
        free(ctx->buf_a);
        free(ctx->buf_b);
        free(ctx->buf_c);
        free(ctx);
    }
}

cnn_result_t cnn_predict(cnn_ctx_t *ctx, const float *input)
{
    cnn_result_t result;
    memset(&result, 0, sizeof(result));

    /* ═══════════════════════════════════════════════════════════
     * Stem: Conv1D(2→16, k7, s2, p3) + GELU → [16, 64]
     * ═══════════════════════════════════════════════════════════ */
    conv1d(input, CNN_INPUT_LENGTH, CNN_STEM_IN_CH, CNN_STEM_OUT_CH,
           CNN_STEM_KERNEL, CNN_STEM_STRIDE, CNN_STEM_PAD,
           onnx__Conv_190, onnx__Conv_191, ctx->buf_a);
    gelu_inplace(ctx->buf_a, CNN_STEM_OUT_CH * CNN_STEM_OUT_LEN);

    /* ═══════════════════════════════════════════════════════════
     * Body Block 0: 16→16 identity, k7 → [16, 64] → MaxPool → [16, 32]
     * ═══════════════════════════════════════════════════════════ */
    body_block(ctx, ctx->buf_a,
               CNN_B0_CONV1_IN_CH, CNN_B0_CONV1_OUT_CH,
               CNN_STEM_OUT_LEN,
               CNN_B0_CONV1_KERNEL, CNN_B0_CONV1_PAD,
               CNN_B0_CONV2_KERNEL, CNN_B0_CONV2_PAD,
               onnx__Conv_193, onnx__Conv_194,
               onnx__Conv_196, onnx__Conv_197,
               NULL, NULL);
    maxpool1d(ctx->buf_b, CNN_B0_CONV2_OUT_LEN, CNN_B0_CONV2_OUT_CH,
              CNN_B0_POOL_KERNEL, CNN_B0_POOL_STRIDE, ctx->buf_a);

    /* ═══════════════════════════════════════════════════════════
     * Body Block 1: 16→32, 1×1 shortcut, k7 → MaxPool → [32, 16]
     * ═══════════════════════════════════════════════════════════ */
    body_block(ctx, ctx->buf_a,
               CNN_B1_CONV1_IN_CH, CNN_B1_CONV1_OUT_CH,
               CNN_B0_OUT_LEN,
               CNN_B1_CONV1_KERNEL, CNN_B1_CONV1_PAD,
               CNN_B1_CONV2_KERNEL, CNN_B1_CONV2_PAD,
               onnx__Conv_202, onnx__Conv_203,
               onnx__Conv_205, onnx__Conv_206,
               onnx__Conv_199, onnx__Conv_200);
    maxpool1d(ctx->buf_b, CNN_B1_CONV2_OUT_LEN, CNN_B1_CONV2_OUT_CH,
              CNN_B1_POOL_KERNEL, CNN_B1_POOL_STRIDE, ctx->buf_a);

    /* ═══════════════════════════════════════════════════════════
     * Body Block 2: 32→48, 1×1 shortcut, k5 → MaxPool → [48, 8]
     * ═══════════════════════════════════════════════════════════ */
    body_block(ctx, ctx->buf_a,
               CNN_B2_CONV1_IN_CH, CNN_B2_CONV1_OUT_CH,
               CNN_B1_OUT_LEN,
               CNN_B2_CONV1_KERNEL, CNN_B2_CONV1_PAD,
               CNN_B2_CONV2_KERNEL, CNN_B2_CONV2_PAD,
               onnx__Conv_211, onnx__Conv_212,
               onnx__Conv_214, onnx__Conv_215,
               onnx__Conv_208, onnx__Conv_209);
    maxpool1d(ctx->buf_b, CNN_B2_CONV2_OUT_LEN, CNN_B2_CONV2_OUT_CH,
              CNN_B2_POOL_KERNEL, CNN_B2_POOL_STRIDE, ctx->buf_a);

    /* ═══════════════════════════════════════════════════════════
     * GlobalAveragePool → [48] features
     * ═══════════════════════════════════════════════════════════ */
    float features[CNN_FEATURE_DIM];
    global_avg_pool1d(ctx->buf_a, CNN_B2_OUT_LEN, CNN_FEATURE_DIM, features);

    /* ═══════════════════════════════════════════════════════════
     * cls_head: FC(48→48)+GELU → FC(48→24)+GELU → FC(24→3)
     * ═══════════════════════════════════════════════════════════ */
    float tmp[48], tmp2[48];
    linear(features, CNN_CLS_FC1_IN, CNN_CLS_FC1_OUT,
           cls_head_0_weight, cls_head_0_bias, tmp);
    gelu_inplace(tmp, CNN_CLS_FC1_OUT);
    linear(tmp, CNN_CLS_FC2_IN, CNN_CLS_FC2_OUT,
           cls_head_3_weight, cls_head_3_bias, tmp2);
    gelu_inplace(tmp2, CNN_CLS_FC2_OUT);
    linear(tmp2, CNN_CLS_FC3_IN, CNN_CLS_FC3_OUT,
           cls_head_6_weight, cls_head_6_bias, result.stage_logits);

    /* ═══════════════════════════════════════════════════════════
     * rul_head: FC(48→48)+GELU → FC(48→24)+GELU → FC(24→1)
     * ═══════════════════════════════════════════════════════════ */
    linear(features, CNN_RUL_FC1_IN, CNN_RUL_FC1_OUT,
           rul_head_0_weight, rul_head_0_bias, tmp);
    gelu_inplace(tmp, CNN_RUL_FC1_OUT);
    linear(tmp, CNN_RUL_FC2_IN, CNN_RUL_FC2_OUT,
           rul_head_3_weight, rul_head_3_bias, tmp2);
    gelu_inplace(tmp2, CNN_RUL_FC2_OUT);
    linear(tmp2, CNN_RUL_FC3_IN, CNN_RUL_FC3_OUT,
           rul_head_6_weight, rul_head_6_bias, &result.rul);

    /* Clamp RUL to [0, 1] */
    if (result.rul < 0.0f) result.rul = 0.0f;
    if (result.rul > 1.0f) result.rul = 1.0f;

    return result;
}
