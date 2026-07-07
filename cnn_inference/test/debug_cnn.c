/*===========================================================================
 * Debug version: dump every layer's output mean/std to compare with numpy
 *===========================================================================*/
#include "cnn_inference.h"
#include "model_weights.h"
#include "model_meta.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define DBG(name, data, n)  printf("  %-35s  n=%-6d  mean=%+12.8f  std=%+12.8f\n", name, n, mean(data,n), std_dev(data,n))

static float mean(const float *x, int n) { float s=0; for(int i=0;i<n;i++) s+=x[i]; return s/(float)n; }
static float std_dev(const float *x, int n) { float m=mean(x,n),s=0; for(int i=0;i<n;i++){float d=x[i]-m;s+=d*d;} return sqrtf(s/(float)n); }

static inline float gelu(float x) { return x * 0.5f * (1.0f + erff(x * 0.7071067811865475f)); }
static void gelu_inplace(float *x, int n) { for (int i = 0; i < n; i++) x[i] = gelu(x[i]); }

/* Same conv1d, maxpool1d, global_avg_pool1d, linear as in cnn_inference.c */
static void conv1d(const float *x, int in_len, int in_ch, int out_ch,
                   int kernel, int stride, int pad,
                   const float *weight, const float *bias, float *out)
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

static void maxpool1d(const float *x, int in_len, int ch, int kernel, int stride, float *out)
{
    int out_len = (in_len - kernel) / stride + 1;
    for (int c = 0; c < ch; c++)
        for (int t = 0; t < out_len; t++) {
            float max_val = -INFINITY;
            for (int k = 0; k < kernel; k++) {
                float val = x[c * in_len + t * stride + k];
                if (val > max_val) max_val = val;
            }
            out[c * out_len + t] = max_val;
        }
}

static void global_avg_pool1d(const float *x, int len, int ch, float *out)
{
    for (int c = 0; c < ch; c++) {
        float sum = 0.0f;
        const float *row = x + c * len;
        for (int t = 0; t < len; t++) sum += row[t];
        out[c] = sum / (float)len;
    }
}

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

int main(void)
{
    /* Use the same input as numpy debug (seed=42, np.random.rand(2,128)) */
    /* Load from file */
    FILE *fp = fopen("/tmp/cnn_debug_input.bin", "rb");
    float input[256];
    if (fp) {
        fread(input, sizeof(float), 256, fp);
        fclose(fp);
    } else {
        printf("ERROR: need /tmp/cnn_debug_input.bin\n");
        printf("Run: python3 -c \"import numpy as np; np.random.seed(42); np.random.rand(2,128).astype(np.float32).tofile('/tmp/cnn_debug_input.bin')\"\n");
        return 1;
    }

    /* Buffers — using VLA or malloc */
    float *buf_a = calloc(4096, sizeof(float));
    float *buf_b = calloc(4096, sizeof(float));
    float *buf_c = calloc(4096, sizeof(float));

    printf("LAYER DEBUG OUTPUT (compare with numpy debug_layers.py)\n");
    printf("============================================================\n\n");

    /* Stem */
    conv1d(input, 128, 2, 16, 7, 2, 3, onnx__Conv_190, onnx__Conv_191, buf_a);
    DBG("STEM conv", buf_a, 16*64);
    gelu_inplace(buf_a, 16*64);
    DBG("STEM GELU", buf_a, 16*64);

    /* Body Block 0 — identity skip, always use separate input/output buffers */
    memcpy(buf_c, buf_a, 16*64*sizeof(float));  /* skip = stem GELU */
    conv1d(buf_a, 64, 16, 16, 7, 1, 3, onnx__Conv_193, onnx__Conv_194, buf_b);
    DBG("B0 conv1", buf_b, 16*64);
    gelu_inplace(buf_b, 16*64);
    DBG("B0 conv1+GELU", buf_b, 16*64);

    conv1d(buf_b, 64, 16, 16, 7, 1, 3, onnx__Conv_196, onnx__Conv_197, buf_a);
    DBG("B0 conv2", buf_a, 16*64);
    gelu_inplace(buf_a, 16*64);
    DBG("B0 conv2+GELU", buf_a, 16*64);

    for (int i = 0; i < 16*64; i++) buf_b[i] = buf_a[i] + buf_c[i];
    DBG("B0 +skip", buf_b, 16*64);

    maxpool1d(buf_b, 64, 16, 2, 2, buf_a);
    DBG("B0 pool", buf_a, 16*32);

    /* Body Block 1 — channel-expanding 1×1 shortcut */
    memcpy(buf_c, buf_a, 16*32*sizeof(float));  /* skip = B0 pool */
    conv1d(buf_a, 32, 16, 32, 7, 1, 3, onnx__Conv_202, onnx__Conv_203, buf_b);
    DBG("B1 conv1", buf_b, 32*32);
    gelu_inplace(buf_b, 32*32);
    conv1d(buf_b, 32, 32, 32, 7, 1, 3, onnx__Conv_205, onnx__Conv_206, buf_a);
    DBG("B1 conv2", buf_a, 32*32);
    gelu_inplace(buf_a, 32*32);
    DBG("B1 conv2+GELU", buf_a, 32*32);

    /* shortcut: 1×1 conv on skip → buf_b, then buf_b += conv2 */
    conv1d(buf_c, 32, 16, 32, 1, 1, 0, onnx__Conv_199, onnx__Conv_200, buf_b);
    DBG("B1 shortcut", buf_b, 32*32);
    for (int i = 0; i < 32*32; i++) buf_b[i] += buf_a[i];
    DBG("B1 +skip", buf_b, 32*32);

    maxpool1d(buf_b, 32, 32, 2, 2, buf_a);
    DBG("B1 pool", buf_a, 32*16);

    /* Body Block 2 — channel-expanding 1×1 shortcut */
    memcpy(buf_c, buf_a, 32*16*sizeof(float));  /* skip = B1 pool */
    conv1d(buf_a, 16, 32, 48, 5, 1, 2, onnx__Conv_211, onnx__Conv_212, buf_b);
    DBG("B2 conv1", buf_b, 48*16);
    gelu_inplace(buf_b, 48*16);
    conv1d(buf_b, 16, 48, 48, 5, 1, 2, onnx__Conv_214, onnx__Conv_215, buf_a);
    DBG("B2 conv2", buf_a, 48*16);
    gelu_inplace(buf_a, 48*16);

    conv1d(buf_c, 16, 32, 48, 1, 1, 0, onnx__Conv_208, onnx__Conv_209, buf_b);
    DBG("B2 shortcut", buf_b, 48*16);
    for (int i = 0; i < 48*16; i++) buf_b[i] += buf_a[i];
    DBG("B2 +skip", buf_b, 48*16);

    maxpool1d(buf_b, 16, 48, 2, 2, buf_a);
    DBG("B2 pool", buf_a, 48*8);

    /* GAP */
    float features[48];
    global_avg_pool1d(buf_a, 8, 48, features);
    DBG("GAP features", features, 48);

    /* Print first 8 features for direct comparison */
    printf("\n  First 8 GAP features: ");
    for (int i = 0; i < 8; i++) printf("%+.6f ", features[i]);
    printf("\n\n");

    /* cls_head */
    float tmp[48], tmp2[48];
    linear(features, 48, 48, cls_head_0_weight, cls_head_0_bias, tmp);
    DBG("cls FC1", tmp, 48);
    gelu_inplace(tmp, 48);
    linear(tmp, 48, 24, cls_head_3_weight, cls_head_3_bias, tmp2);
    DBG("cls FC2", tmp2, 24);
    gelu_inplace(tmp2, 24);
    float stage_logits[3];
    linear(tmp2, 24, 3, cls_head_6_weight, cls_head_6_bias, stage_logits);
    printf("  cls stage_logits: %+.6f %+.6f %+.6f\n", stage_logits[0], stage_logits[1], stage_logits[2]);

    /* rul_head */
    linear(features, 48, 48, rul_head_0_weight, rul_head_0_bias, tmp);
    gelu_inplace(tmp, 48);
    linear(tmp, 48, 24, rul_head_3_weight, rul_head_3_bias, tmp2);
    gelu_inplace(tmp2, 24);
    float rul;
    linear(tmp2, 24, 1, rul_head_6_weight, rul_head_6_bias, &rul);
    printf("  rul: %+.6f\n", rul);

    free(buf_a); free(buf_b); free(buf_c);
    return 0;
}
