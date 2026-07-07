/*===========================================================================
 * CnnPreprocessor.cpp — CNN input preprocessing implementation
 *
 * Replicates battery_infer.c preprocessing EXACTLY. Must produce byte-identical
 * output for the same raw IC input.
 *===========================================================================*/
#include "CnnPreprocessor.h"
#include "scalers.h"       /* CNN_IC_SCALER_MEAN, CNN_IC_SCALER_STD, CNN_IG_SCALER_MEAN, CNN_IG_SCALER_STD */
#include <cmath>
#include <algorithm>

void CnnPreprocessor::normalizeIC(const float raw_ic[128], float normalized[128])
{
    for (int i = 0; i < 128; i++) {
        float val = raw_ic[i];
        /* Handle NaN/Inf */
        if (std::isnan(val) || std::isinf(val))
            val = 0.0f;
        /* StandardScaler: (x - mean) / std */
        val = (val - CNN_IC_SCALER_MEAN[i]) / CNN_IC_SCALER_STD[i];
        /* Clip to [-5, 5] */
        if (val > 5.0f)  val = 5.0f;
        if (val < -5.0f) val = -5.0f;
        normalized[i] = val;
    }
}

void CnnPreprocessor::computeGradient(const float normalized[128],
                                       float gradient[128], float &abs_max_out)
{
    /* Central difference (matches np.gradient with edge_order=1):
     *   gradient[0]   = normalized[1] - normalized[0]                 (forward at left edge)
     *   gradient[i]   = (normalized[i+1] - normalized[i-1]) * 0.5     (central, interior)
     *   gradient[127] = normalized[127] - normalized[126]             (backward at right edge)
     */
    gradient[0] = normalized[1] - normalized[0];
    float abs_max = std::fabs(gradient[0]);
    for (int i = 1; i < 127; i++) {
        float g = (normalized[i + 1] - normalized[i - 1]) * 0.5f;
        gradient[i] = g;
        float ag = std::fabs(g);
        if (ag > abs_max) abs_max = ag;
    }
    gradient[127] = normalized[127] - normalized[126];
    float ag = std::fabs(gradient[127]);
    if (ag > abs_max) abs_max = ag;

    if (abs_max < 1e-6f) abs_max = 1.0f;
    abs_max_out = abs_max;
}

void CnnPreprocessor::normalizeGradient(const float gradient[128], float abs_max,
                                         float normalized[128])
{
    for (int i = 0; i < 128; i++) {
        /* Divide by abs_max, then StandardScaler */
        float val = gradient[i] / abs_max;
        if (std::isnan(val) || std::isinf(val))
            val = 0.0f;
        val = (val - CNN_IG_SCALER_MEAN[i]) / CNN_IG_SCALER_STD[i];
        /* Clip to [-5, 5] */
        if (val > 5.0f)  val = 5.0f;
        if (val < -5.0f) val = -5.0f;
        normalized[i] = val;
    }
}

void CnnPreprocessor::process(const float raw_ic[128], float output[256])
{
    float norm_ic[128];
    float gradient[128];
    float norm_gradient[128];
    float abs_max;

    /* Stage 1: normalize IC curve */
    normalizeIC(raw_ic, norm_ic);

    /* Stage 2: compute gradient */
    computeGradient(norm_ic, gradient, abs_max);

    /* Stage 3: normalize gradient */
    normalizeGradient(gradient, abs_max, norm_gradient);

    /* Stage 4: stack as dual-channel
     *   channel 0 [0..127]:   normalized IC
     *   channel 1 [128..255]: normalized gradient
     */
    for (int i = 0; i < 128; i++) {
        output[i]       = norm_ic[i];
        output[128 + i] = norm_gradient[i];
    }
}
