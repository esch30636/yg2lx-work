/*===========================================================================
 * InferenceEngine.cpp — C++ wrapper implementation
 *
 * Includes the C inference headers directly. Compiled as C++ but the C
 * headers use standard types compatible with both languages.
 *===========================================================================*/
#include "InferenceEngine.h"
#include "CnnPreprocessor.h"

/* Include C inference API */
extern "C" {
#include "battery_inference.h"
}

#include <cmath>
#include <cstring>

InferenceEngine::InferenceEngine()
    : m_pinn_ctx(nullptr)
    , m_cnn_ctx(nullptr)
    , m_ready(false)
{
    m_pinn_ctx = pinn_init();
    if (!m_pinn_ctx) {
        m_lastError = "pinn_init() failed: memory allocation error";
        return;
    }

    m_cnn_ctx = cnn_init();
    if (!m_cnn_ctx) {
        m_lastError = "cnn_init() failed: memory allocation error";
        pinn_free(static_cast<pinn_ctx_t*>(m_pinn_ctx));
        m_pinn_ctx = nullptr;
        return;
    }

    m_ready = true;
}

InferenceEngine::~InferenceEngine()
{
    if (m_pinn_ctx) pinn_free(static_cast<pinn_ctx_t*>(m_pinn_ctx));
    if (m_cnn_ctx)  cnn_free(static_cast<cnn_ctx_t*>(m_cnn_ctx));
}

float InferenceEngine::predictSOH(const float features[132])
{
    if (!m_ready) return 0.0f;
    return pinn_predict(static_cast<pinn_ctx_t*>(m_pinn_ctx), features);
}

CnnResult InferenceEngine::predictStageAndRUL(const float ic_curve[128])
{
    CnnResult out;
    std::memset(&out, 0, sizeof(out));

    if (!m_ready) return out;

    /* Preprocess raw IC curve → dual-channel [256] */
    float dual_channel[256];
    CnnPreprocessor::process(ic_curve, dual_channel);

    /* Run CNN inference */
    cnn_result_t raw = cnn_predict(static_cast<cnn_ctx_t*>(m_cnn_ctx), dual_channel);

    /* Decode stage via argmax */
    int stage = 0;
    if (raw.stage_logits[1] > raw.stage_logits[stage]) stage = 1;
    if (raw.stage_logits[2] > raw.stage_logits[stage]) stage = 2;
    out.stage = stage;

    /* Copy raw logits */
    out.stage_logits[0] = raw.stage_logits[0];
    out.stage_logits[1] = raw.stage_logits[1];
    out.stage_logits[2] = raw.stage_logits[2];

    /* Softmax for probabilities */
    float max_logit = raw.stage_logits[0];
    if (raw.stage_logits[1] > max_logit) max_logit = raw.stage_logits[1];
    if (raw.stage_logits[2] > max_logit) max_logit = raw.stage_logits[2];

    float sum_exp = 0.0f;
    for (int i = 0; i < 3; i++) {
        out.stage_probs[i] = std::exp(raw.stage_logits[i] - max_logit);
        sum_exp += out.stage_probs[i];
    }
    if (sum_exp > 1e-10f) {
        for (int i = 0; i < 3; i++)
            out.stage_probs[i] /= sum_exp;
    } else {
        /* Degenerate case: all logits nearly equal → uniform */
        out.stage_probs[0] = out.stage_probs[1] = out.stage_probs[2] = 1.0f / 3.0f;
    }

    out.rul = raw.rul;
    return out;
}
