/*===========================================================================
 * InferenceWorker.h — QThread worker for CNN inference
 *
 * Runs CNN inference (~45ms) on a dedicated thread to avoid blocking
 * the main QApplication event loop. Uses queued signal/slot connections
 * for thread-safe data transfer.
 *
 * PINN inference (~3.5ms) runs on the main thread since it's fast enough
 * not to cause visible frame drops.
 *===========================================================================*/
#ifndef HMI_THREADS_INFERENCE_WORKER_H
#define HMI_THREADS_INFERENCE_WORKER_H

#include <QObject>
#include <QThread>
#include <QVector>
#include "../inference/InferenceEngine.h"

class InferenceWorker : public QObject {
    Q_OBJECT
public:
    explicit InferenceWorker(QObject *parent = nullptr);
    ~InferenceWorker() override;

    bool isReady() const { return m_engine.isReady(); }

public slots:
    /** Called from main thread via QueuedConnection */
    void runCnn(const QVector<float> &ic_curve_128);

signals:
    /** Emitted back to main thread with results */
    void cnnResultReady(int stage, QVector<float> probs, float rul);

    /** Emitted on error */
    void cnnError(const QString &message);

private:
    InferenceEngine m_engine;   /* separate instance for worker thread */
};

#endif /* HMI_THREADS_INFERENCE_WORKER_H */
