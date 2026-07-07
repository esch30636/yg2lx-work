/*===========================================================================
 * cnn_inference.c — Battery SOH CNN Inference Engine
 *
 * Pure C implementation matching ONNX graph operations exactly.
 * All weights pre-dequantized to float32 at build time.
 *
 * Architecture:
 *   Input [1, 2, 128]
 *   → Stem: Conv1D(2→16,k7,s2) + GELU                              → [1,16,64]
 *   → Body0: Conv1D(16→16,k7)+GELU + Conv1D(16→16,k7)+GELU + skip  → [1,16,64]
 *   → MaxPool(k2)                                                   → [1,16,32]
 *   → Body1: SC(16→32,k1) + Conv(16→32,k7)+GELU + Conv(32→32)+GELU → [1,32,32]
 *   → MaxPool(k2)                                                   → [1,32,16]
 *   → Body2: SC(32→48,k1) + Conv(32→48,k5)+GELU + Conv(48→48)+GELU → [1,48,16]
 *   → MaxPool(k2)                                                   → [1,48,8]
 *   → GlobalAvgPool → [1,48,1] → Squeeze → [48]
 *   → cls_head: FC(48→48)+GELU → FC(48→24)+GELU → FC(24→3) → stage_logits[3]
 *   → rul_head: FC(48→48)+GELU → FC(48→24)+GELU → FC(24→1) → rul
 *===========================================================================*/

#include "cnn_inference.h"
#include "model_weights.h"
#include "model_meta.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================
 * Internal context
 *===========================================================================*/
struct cnn_ctx {
    float *buf_a;    /* primary scratch buffer */
    float *buf_b;    /* secondary scratch buffer */
    float *buf_c;    /* tertiary scratch buffer (for residual skip) */
};

/*===========================================================================
 * Math helpers
 *===========================================================================*/

/** GELU activation: x * 0.5 * (1 + erf(x / sqrt(2))) */
static inline float gelu(float x)
{
    return x * 0.5f * (1.0f + erff(x * 0.7071067811865475f));  /* 1/sqrt(2) */
}

/** Apply GELU element-wise over n elements */
static void gelu_inplace(float *x, int n)
{
    for (int i = 0; i < n; i++)
        x[i] = gelu(x[i]);
}

/*===========================================================================
 * 1D Convolution — NCW layout
 *
 * Weight layout: [out_channels][in_channels][kernel_size]
 *   stored flat as: w[((oc * in_ch) + ic) * ks + k]
 *
 * Input layout:  [channels][length]  (batch=1 implicit)
 *   stored flat as: x[ch * len + t]
 *
 * Output layout: [out_channels][out_length]
 *===========================================================================*/

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
                        int x_idx = ic * in_len + pos;
                        sum += x[x_idx] * weight[w_idx];
                    }
                }
            }
            out[oc * out_len + t] = sum;
        }
    }
}

/*===========================================================================
 * MaxPool1D
 *   kernel_shape, stride, no padding (as per ONNX graph)
 *===========================================================================*/

static void maxpool1d(const float *x, int in_len, int ch,
                      int kernel, int stride,
                      float *out)
{
    int out_len = (in_len - kernel) / stride + 1;

    for (int c = 0; c < ch; c++) {
        for (int t = 0; t < out_len; t++) {
            float max_val = -INFINITY;
            for (int k = 0; k < kernel; k++) {
                int pos = t * stride + k;
                float val = x[c * in_len + pos];
                if (val > max_val) max_val = val;
            }
            out[c * out_len + t] = max_val;
        }
    }
}

/*===========================================================================
 * GlobalAveragePool1D → reduce each channel to scalar
 *   Input:  [ch][len] → Output: [ch][1]
 *===========================================================================*/

static void global_avg_pool1d(const float *x, int len, int ch, float *out)
{
    for (int c = 0; c < ch; c++) {
        float sum = 0.0f;
        const float *row = x + c * len;
        for (int t = 0; t < len; t++)
            sum += row[t];
        out[c] = sum / (float)len;
    }
}

/*===========================================================================
 * Fully Connected (Linear) layer
 *   Weight layout: [out_features][in_features] flat row-major
 *===========================================================================*/

static void linear(const float *x, int in_dim, int out_dim,
                   const float *weight, const float *bias,
                   float *out)
{
    /* ONNX Gemm: weight layout is [in_features][out_features] */
    for (int o = 0; o < out_dim; o++) {
        float sum = bias ? bias[o] : 0.0f;
        for (int i = 0; i < in_dim; i++)
            sum += x[i] * weight[i * out_dim + o];
        out[o] = sum;
    }
}

/*===========================================================================
 * ResNet building blocks — hardcoded to match the ONNX graph exactly
 *===========================================================================*/

/**
 * Stem block: Conv1D(2→16, k7, s2, p3) + GELU
 */
static void stem_block(cnn_ctx_t *ctx, const float *input)
{
    conv1d(input, MODEL_INPUT_LENGTH, STEM_CONV_IN_CH, STEM_CONV_OUT_CH,
           STEM_CONV_KERNEL, STEM_CONV_STRIDE, STEM_CONV_PAD,
           onnx__Conv_190, onnx__Conv_191,
           ctx->buf_a);  /* → [16, 64] */
    gelu_inplace(ctx->buf_a, STEM_CONV_OUT_CH * STEM_OUT_LENGTH);
}

/**
 * Body Block with optional channel-expansion shortcut.
 *
 * Pattern:
 *   conv1(ich→och) + GELU + conv2(och→och) + GELU
 *   + shortcut (identity if ich==och, else 1×1 conv)
 *   → output in buf_a, skip stored in buf_c
 */
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

    /* Save input for skip connection (buf_c) */
    int in_size = in_ch * in_len;
    memcpy(ctx->buf_c, block_input, in_size * sizeof(float));

    /*
     * CRITICAL: conv1 writes to buf_b (NOT buf_a), because block_input
     * may alias ctx->buf_a.  In-place convolution with stride=1 corrupts
     * the input before downstream positions are read.
     */
    conv1d(block_input, in_len, in_ch, out_ch,
           conv1_kernel, 1, conv1_pad,
           conv1_w, conv1_b,
           ctx->buf_b);                              /* buf_b ← conv1 */
    gelu_inplace(ctx->buf_b, out_ch * conv1_out_len);

    /* Conv2: buf_b → buf_a (separate buffers, no aliasing) */
    conv1d(ctx->buf_b, conv1_out_len, out_ch, out_ch,
           conv2_kernel, 1, conv2_pad,
           conv2_w, conv2_b,
           ctx->buf_a);                              /* buf_a ← conv2 */
    gelu_inplace(ctx->buf_a, out_ch * conv2_out_len);

    /*
     * Shortcut + final result → ctx->buf_b
     *   buf_b  =  conv2_out  +  skip
     *           =  buf_a     +  (buf_c or shortcut_conv)
     */
    if (in_ch == out_ch) {
        /* Identity: buf_b = buf_a + buf_c */
        for (int i = 0; i < out_ch * conv2_out_len; i++)
            ctx->buf_b[i] = ctx->buf_a[i] + ctx->buf_c[i];
    } else {
        /* 1×1 conv: shortcut_conv(buf_c) → buf_b, then buf_b += buf_a */
        conv1d(ctx->buf_c, in_len, in_ch, out_ch,
               1, 1, 0,
               sc_w, sc_b,
               ctx->buf_b);
        for (int i = 0; i < out_ch * conv2_out_len; i++)
            ctx->buf_b[i] += ctx->buf_a[i];
    }
    /* Result now in ctx->buf_b (caller expects this for maxpool) */
}

/**
 * Classification head: FC(48→48)+GELU → FC(48→24)+GELU → FC(24→3)
 */
static void cls_head(cnn_ctx_t *ctx, const float *features, float *stage_logits)
{
    float *buf = ctx->buf_a;
    float *buf2 = ctx->buf_b;

    /* FC1: 48 → 48 */
    linear(features, CLS_FC1_IN, CLS_FC1_OUT,
           cls_head_0_weight, cls_head_0_bias, buf);
    gelu_inplace(buf, CLS_FC1_OUT);

    /* FC2: 48 → 24 */
    linear(buf, CLS_FC2_IN, CLS_FC2_OUT,
           cls_head_3_weight, cls_head_3_bias, buf2);
    gelu_inplace(buf2, CLS_FC2_OUT);

    /* FC3: 24 → 3 */
    linear(buf2, CLS_FC3_IN, CLS_FC3_OUT,
           cls_head_6_weight, cls_head_6_bias, stage_logits);
}

/**
 * RUL regression head: FC(48→48)+GELU → FC(48→24)+GELU → FC(24→1)
 */
static void rul_head(cnn_ctx_t *ctx, const float *features, float *rul)
{
    float *buf = ctx->buf_a;
    float *buf2 = ctx->buf_b;

    /* FC1: 48 → 48 */
    linear(features, RUL_FC1_IN, RUL_FC1_OUT,
           rul_head_0_weight, rul_head_0_bias, buf);
    gelu_inplace(buf, RUL_FC1_OUT);

    /* FC2: 48 → 24 */
    linear(buf, RUL_FC2_IN, RUL_FC2_OUT,
           rul_head_3_weight, rul_head_3_bias, buf2);
    gelu_inplace(buf2, RUL_FC2_OUT);

    /* FC3: 24 → 1 */
    linear(buf2, RUL_FC3_IN, RUL_FC3_OUT,
           rul_head_6_weight, rul_head_6_bias, rul);
}

/*===========================================================================
 * Public API
 *===========================================================================*/

cnn_ctx_t* cnn_init(void)
{
    cnn_ctx_t *ctx = calloc(1, sizeof(cnn_ctx_t));
    if (!ctx) return NULL;

    ctx->buf_a = calloc(MAX_BUF_SIZE, sizeof(float));
    ctx->buf_b = calloc(MAX_BUF_SIZE, sizeof(float));
    ctx->buf_c = calloc(MAX_BUF_SIZE, sizeof(float));

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

cnn_result_t cnn_infer(cnn_ctx_t *ctx, const float *input)
{
    cnn_result_t result;
    memset(&result, 0, sizeof(result));

    /* ══════════════════════════════════════════════════════════════
     * Stem: Conv1D(2→16, k7, s2, p3) + GELU
     *   Input:  [2, 128] → Output: [16, 64]
     * ══════════════════════════════════════════════════════════════ */
    stem_block(ctx, input);

    /* ══════════════════════════════════════════════════════════════
     * Body Block 0: 16→16, identity skip, k7
     *   Then MaxPool(k2,s2): [16, 64] → [16, 32]
     * ══════════════════════════════════════════════════════════════ */
    body_block(ctx, ctx->buf_a,
               B0_CONV1_IN_CH, B0_CONV1_OUT_CH,
               STEM_OUT_LENGTH,
               B0_CONV1_KERNEL, B0_CONV1_PAD,
               B0_CONV2_KERNEL, B0_CONV2_PAD,
               onnx__Conv_193, onnx__Conv_194,
               onnx__Conv_196, onnx__Conv_197,
               NULL, NULL);

    /* After body_block: result in buf_b [16, 64]. MaxPool it. */
    maxpool1d(ctx->buf_b, B0_CONV2_OUT_LEN, B0_CONV2_OUT_CH,
              B0_POOL_KERNEL, B0_POOL_STRIDE, ctx->buf_a);  /* [16, 32] */

    /* ══════════════════════════════════════════════════════════════
     * Body Block 1: 16→32, 1×1 shortcut, k7
     *   Then MaxPool(k2,s2): [32, 32] → [32, 16]
     * ══════════════════════════════════════════════════════════════ */
    body_block(ctx, ctx->buf_a,
               B1_CONV1_IN_CH, B1_CONV1_OUT_CH,
               B0_OUT_LENGTH,
               B1_CONV1_KERNEL, B1_CONV1_PAD,
               B1_CONV2_KERNEL, B1_CONV2_PAD,
               onnx__Conv_202, onnx__Conv_203,
               onnx__Conv_205, onnx__Conv_206,
               onnx__Conv_199, onnx__Conv_200);

    maxpool1d(ctx->buf_b, B1_CONV2_OUT_LEN, B1_CONV2_OUT_CH,
              B1_POOL_KERNEL, B1_POOL_STRIDE, ctx->buf_a);  /* [32, 16] */

    /* ══════════════════════════════════════════════════════════════
     * Body Block 2: 32→48, 1×1 shortcut, k5
     *   Then MaxPool(k2,s2): [48, 16] → [48, 8]
     * ══════════════════════════════════════════════════════════════ */
    body_block(ctx, ctx->buf_a,
               B2_CONV1_IN_CH, B2_CONV1_OUT_CH,
               B1_OUT_LENGTH,
               B2_CONV1_KERNEL, B2_CONV1_PAD,
               B2_CONV2_KERNEL, B2_CONV2_PAD,
               onnx__Conv_211, onnx__Conv_212,
               onnx__Conv_214, onnx__Conv_215,
               onnx__Conv_208, onnx__Conv_209);

    maxpool1d(ctx->buf_b, B2_CONV2_OUT_LEN, B2_CONV2_OUT_CH,
              B2_POOL_KERNEL, B2_POOL_STRIDE, ctx->buf_a);  /* [48, 8] */

    /* ══════════════════════════════════════════════════════════════
     * GlobalAveragePool → [48, 1] → features [48]
     * ══════════════════════════════════════════════════════════════ */
    float features[FEATURE_DIM];
    global_avg_pool1d(ctx->buf_a, B2_OUT_LENGTH, FEATURE_DIM, features);

    /* ══════════════════════════════════════════════════════════════
     * Dual heads (shared features, independent paths)
     * ══════════════════════════════════════════════════════════════ */
    cls_head(ctx, features, result.stage_logits);
    rul_head(ctx, features, &result.rul);

    return result;
}
