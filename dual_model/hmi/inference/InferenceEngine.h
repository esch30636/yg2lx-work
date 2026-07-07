/*===========================================================================
 * InferenceEngine.h — C++ wrapper around the pure C dual-model inference API
 *
 * Wraps pinn_init/pinn_predict/pinn_free and cnn_init/cnn_predict/cnn_free.
 * Handles context lifecycle via RAII. Not thread-safe — use separate
 * instances for concurrent access.
 *
 * Usage:
 *   InferenceEngine engine;
 *   float soh = engine.predictSOH(features_132);
 *   auto [stage, probs, rul] = engine.predictStageAndRUL(ic_curve_128);
 *===========================================================================*/
#ifndef HMI_INFERENCE_ENGINE_H
#define HMI_INFERENCE_ENGINE_H

#include <stdexcept>
#include <string>

/* ── CNN result (with softmax probabilities) ── */
struct CnnResult {
    int stage;               /* 0=healthy, 1=degrading, 2=EOL */
    float stage_probs[3];    /* softmax(stage_logits) */
    float stage_logits[3];   /* raw logits from model */
    float rul;               /* remaining useful life ∈ [0, 1] */
};

/* ── PINN result ── */
struct PinnResult {
    float soh;               /* state of health ∈ [0, 1] */
};

class InferenceEngine {
public:
    /**
     * Initialize both PINN and CNN inference contexts.
     * Throws std::runtime_error if memory allocation fails.
     */
    InferenceEngine();

    ~InferenceEngine();

    /* Non-copyable, non-movable (owns C contexts) */
    InferenceEngine(const InferenceEngine &) = delete;
    InferenceEngine &operator=(const InferenceEngine &) = delete;

    /**
     * PINN: Fast SOH screening.
     *
     * @param features  [132] raw (un-normalized) features:
     *                  [0..127]   IC curve on voltage grid
     *                  [128]      temperature (°C)
     *                  [129]      log10(cycle_count + 1)
     *                  [130]      dV proxy (start voltage drop)
     *                  [131]      measured capacity
     * @return SOH ∈ [0, 1]
     */
    float predictSOH(const float features[132]);

    /**
     * CNN: Precise stage classification + RUL prediction.
     *
     * @param ic_curve  [128] raw IC curve values (dQ/dV).
     *                  Preprocessing is applied internally.
     * @return CnnResult with stage, softmax probs, and RUL
     */
    CnnResult predictStageAndRUL(const float ic_curve[128]);

    /** Check if inference contexts were initialized successfully */
    bool isReady() const { return m_ready; }

    /** Get last error message */
    const std::string &lastError() const { return m_lastError; }

private:
    void *m_pinn_ctx;    /* pinn_ctx_t* — opaque C pointer */
    void *m_cnn_ctx;     /* cnn_ctx_t*  — opaque C pointer */
    bool m_ready;
    std::string m_lastError;
};

#endif /* HMI_INFERENCE_ENGINE_H */
