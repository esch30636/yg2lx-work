/*===========================================================================
 * CnnPreprocessor.h — CNN input preprocessing pipeline
 *
 * Transforms raw 128-point IC curve into 256-element dual-channel input
 * matching the ONNX Runtime preprocessing exactly.
 *
 * Pipeline (must match battery_infer.c lines 78-105 byte-for-byte):
 *   1. Normalize IC:  (raw[i] - mean[i]) / std[i], clip[-5,5]
 *   2. Gradient:      g[i] = normalized[i] - normalized[i-1]
 *   3. Normalize grad: g[i]/abs_max, then (g[i] - mean[i]) / std[i], clip[-5,5]
 *   4. Stack:         output[0..127]=norm_ic, output[128..255]=norm_gradient
 *===========================================================================*/
#ifndef HMI_CNN_PREPROCESSOR_H
#define HMI_CNN_PREPROCESSOR_H

#include <cstddef>

class CnnPreprocessor {
public:
    /**
     * Full preprocessing pipeline: raw 128-point IC curve → dual-channel [256].
     *
     * @param raw_ic    [128] raw IC curve values (dQ/dV)
     * @param output    [256] preprocessed dual-channel input for cnn_predict()
     */
    static void process(const float raw_ic[128], float output[256]);

    /* Individual pipeline stages (exposed for verification/testing) */

    /** Stage 1: StandardScaler normalize IC curve, clip [-5, 5] */
    static void normalizeIC(const float raw_ic[128], float normalized[128]);

    /** Stage 2: Forward-difference gradient of normalized IC */
    static void computeGradient(const float normalized[128], float gradient[128],
                                float &abs_max_out);

    /** Stage 3: Abs-max normalize + StandardScaler normalize gradient, clip [-5, 5] */
    static void normalizeGradient(const float gradient[128], float abs_max,
                                  float normalized[128]);
};

#endif /* HMI_CNN_PREPROCESSOR_H */
