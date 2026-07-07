/* Debug: layer-by-layer output comparison for PINN */
#include "battery_inference.h"
#include "pinn_weights.h"
#include "pinn_meta.h"
#include "scalers.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define DBG(name, data, n) \
    printf("  %-30s n=%-4d mean=%+12.8f std=%+12.8f\n", name, n, _mean(data,n), _std(data,n))

static float _mean(const float *x, int n) { float s=0; for(int i=0;i<n;i++) s+=x[i]; return s/(float)n; }
static float _std(const float *x, int n) { float m=_mean(x,n),s=0; for(int i=0;i<n;i++){float d=x[i]-m;s+=d*d;} return sqrtf(s/(float)n); }

static inline float gelu(float x) { return x*0.5f*(1.0f+erff(x*0.7071067811865475f)); }
static void gelu_inplace(float *x, int n) { for(int i=0;i<n;i++) x[i]=gelu(x[i]); }
static inline float sigmoid(float x) {
    if(x>=0) return 1.0f/(1.0f+expf(-x));
    else { float ex=expf(x); return ex/(1.0f+ex); }
}

static void linear(const float *x, int in_dim, int out_dim,
                   const float *weight, const float *bias, float *out) {
    for(int o=0;o<out_dim;o++){
        float sum=bias?bias[o]:0.0f;
        for(int i=0;i<in_dim;i++) sum+=x[i]*weight[i*out_dim+o];
        out[o]=sum;
    }
}

static void layernorm(const float *x, int dim,
                      const float *gamma, const float *beta, float *out) {
    float mean=0.0f;
    for(int i=0;i<dim;i++) mean+=x[i];
    mean/=(float)dim;
    float var=0.0f;
    for(int i=0;i<dim;i++){float d=x[i]-mean; var+=d*d;}
    var/=(float)dim;
    float inv_std=1.0f/sqrtf(var+LAYERNORM_EPS);
    for(int i=0;i<dim;i++){
        float g=gamma?gamma[i]:1.0f, b=beta?beta[i]:0.0f;
        out[i]=(x[i]-mean)*inv_std*g+b;
    }
}

int main(void) {
    /* Load same input as verify_accuracy.py (seed=42, randn) */
    float input[132];
    FILE *fp = fopen("/tmp/pinn_test_input.bin", "rb");
    if(!fp) { printf("ERROR: need /tmp/pinn_test_input.bin\n"); return 1; }
    fread(input, sizeof(float), 132, fp);
    fclose(fp);
    DBG("RAW INPUT", input, 132);

    float buf[132], tmp[132];

    /* Normalize */
    for(int i=0;i<132;i++){
        float val=input[i];
        if(isnan(val)||isinf(val)) val=0.0f;
        val=(val-PINN_SCALER_MEAN[i])/PINN_SCALER_STD[i];
        if(val>5.0f)val=5.0f; if(val<-5.0f)val=-5.0f;
        buf[i]=val;
    }
    DBG("NORMALIZED", buf, 132);

    /* Encoder 0 */
    linear(buf, 132, 128, encoder_0_weight, encoder_0_bias, tmp);
    DBG("enc0 linear", tmp, 128);
    layernorm(tmp, 128, encoder_1_weight, encoder_1_bias, buf);
    DBG("enc0 LN", buf, 128);
    gelu_inplace(buf, 128);
    DBG("enc0 GELU", buf, 128);

    /* Encoder 1 */
    linear(buf, 128, 128, encoder_4_weight, encoder_4_bias, tmp);
    DBG("enc1 linear", tmp, 128);
    layernorm(tmp, 128, encoder_5_weight, encoder_5_bias, buf);
    DBG("enc1 LN", buf, 128);
    gelu_inplace(buf, 128);
    DBG("enc1 GELU", buf, 128);

    /* Encoder 2 */
    linear(buf, 128, 64, encoder_8_weight, encoder_8_bias, tmp);
    DBG("enc2 linear", tmp, 64);
    memcpy(buf, tmp, 64*sizeof(float));

    /* Residual block */
    float skip[64];
    memcpy(skip, buf, 64*sizeof(float));

    linear(buf, 64, 64, res_block_fc1_weight, res_block_fc1_bias, tmp);
    DBG("res fc1", tmp, 64);
    layernorm(tmp, 64, res_block_norm1_weight, res_block_norm1_bias, buf);
    DBG("res norm1", buf, 64);
    gelu_inplace(buf, 64);
    DBG("res GELU1", buf, 64);

    linear(buf, 64, 64, res_block_fc2_weight, res_block_fc2_bias, tmp);
    DBG("res fc2", tmp, 64);
    layernorm(tmp, 64, res_block_norm2_weight, res_block_norm2_bias, buf);
    DBG("res norm2", buf, 64);

    for(int i=0;i<64;i++) buf[i]=gelu(skip[i]+buf[i]);
    DBG("res +skip+GELU", buf, 64);

    /* Head */
    linear(buf, 64, 32, head_0_weight, head_0_bias, tmp);
    DBG("head fc1", tmp, 32);
    layernorm(tmp, 32, head_1_weight, head_1_bias, buf);
    DBG("head LN", buf, 32);
    gelu_inplace(buf, 32);
    DBG("head GELU", buf, 32);

    float logit;
    linear(buf, 32, 1, head_3_weight, head_3_bias, &logit);
    printf("  logit: %+.8f\n", logit);

    float soh = sigmoid(logit);
    printf("  SOH: %.8f\n", soh);

    return 0;
}
