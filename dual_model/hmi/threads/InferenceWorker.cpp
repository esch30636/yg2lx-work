/*===========================================================================
 * InferenceWorker.cpp — CNN worker thread implementation
 *===========================================================================*/
#include "InferenceWorker.h"
#include <cstring>

InferenceWorker::InferenceWorker(QObject *parent)
    : QObject(parent)
{
}

InferenceWorker::~InferenceWorker() = default;

void InferenceWorker::runCnn(const QVector<float> &ic_curve_128)
{
    if (!m_engine.isReady()) {
        emit cnnError("CNN inference engine not initialized");
        return;
    }

    /* QVector is already a deep copy (queued connection copies it).
     * Copy to local array for the inference engine. */
    float local_ic[128];
    std::memcpy(local_ic, ic_curve_128.constData(), 128 * sizeof(float));

    /* Run inference (this takes ~45ms on Cortex-A55) */
    CnnResult result = m_engine.predictStageAndRUL(local_ic);

    /* Copy probs to QVector for safe cross-thread emission */
    QVector<float> probs(3);
    probs[0] = result.stage_probs[0];
    probs[1] = result.stage_probs[1];
    probs[2] = result.stage_probs[2];

    /* Emit result back to main thread */
    emit cnnResultReady(result.stage, probs, result.rul);
}
