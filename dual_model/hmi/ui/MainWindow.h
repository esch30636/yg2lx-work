/*===========================================================================
 * MainWindow.h — Central HMI orchestrator (v3.0 — 1920×1080 oscilloscope)
 *
 * Owns and coordinates:
 *   - DataProvider (SPI / file / demo)
 *   - InferenceEngine (main-thread PINN)
 *   - InferenceWorker (dedicated thread CNN)
 *   - QStackedWidget with 3 screens:
 *       Screen 0: InitScreen      — progress bar + "Start Test"
 *       Screen 1: ModelSelectScreen — PINN vs CNN choice
 *       Screen 2: ResultScreen    — oscilloscope V/A chart + digital readouts
 *
 * Flow:
 *   InitScreen → startTestClicked → ModelSelectScreen
 *   ModelSelectScreen → modelSelected → ResultScreen (timers start)
 *   ResultScreen → backClicked → ModelSelectScreen (timers stop)
 *
 * v3.0: Default 1920×1080 fullscreen, light theme, oscilloscope V/A display.
 *       Voltage (blue) + Current (red) vs Time — primary chart.
 *       IC curve still available but secondary.
 *===========================================================================*/
#ifndef HMI_UI_MAIN_WINDOW_H
#define HMI_UI_MAIN_WINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QTimer>
#include <QThread>
#include <QLabel>
#include <QElapsedTimer>
#include <deque>

#include "../data/DataProvider.h"
#include "../inference/InferenceEngine.h"
#include "../inference/SohAccumulator.h"
#include "../threads/InferenceWorker.h"
#include "screens/InitScreen.h"
#include "screens/ModelSelectScreen.h"
#include "screens/ResultScreen.h"
#include "AlarmIndicator.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(DataProvider *provider, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    /* Screen transitions */
    void onStartTest();
    void onModelSelected(int model);
    void onBackToModelSelect();
    void onStopRequested();

    /* Data & inference */
    void onDataAcquisition();
    void onPinnInference();
    void onCnnResult(int stage, QVector<float> probs, float rul);
    void onCnnError(const QString &msg);
    void onClockUpdate();

signals:
    /** Cross-thread signal: carries IC curve copy to the worker */
    void requestCnnInference(const QVector<float> &ic_curve);

private:
    void setupUi();
    void setupWorkerThread();
    void startAcquisition(int model);
    void stopAcquisition();
    void checkAlarms(const BatterySample &sample);
    void onPinnConverged(float finalSoh, float finalCiHalf,
                         int samples, float stddev, bool natural);
    void onCnnConverged(float finalRul, float finalConfidence,
                        int finalStage, int inferenceCount, bool natural);
    void applyTheme();

    /* Owned components */
    DataProvider       *m_dataProvider;
    InferenceEngine    *m_inferenceEngine;
    InferenceWorker    *m_worker;
    QThread            *m_workerThread;
    SohAccumulator      m_sohAccumulator;

    /* Stacked widget + screens */
    QStackedWidget     *m_stack;
    InitScreen         *m_initScreen;
    ModelSelectScreen  *m_modelSelectScreen;
    ResultScreen       *m_resultScreen;

    /* Header widgets */
    QLabel             *m_headerTitle;
    QLabel             *m_headerClock;
    AlarmIndicator     *m_alarmIndicator;

    /* Timers */
    QTimer *m_dataTimer;
    QTimer *m_pinnTimer;
    QTimer *m_cnnTimer;
    QTimer *m_clockTimer;

    /* State */
    BatterySample m_latestSample;
    AlarmState    m_alarmState;
    double        m_elapsedSec;
    QElapsedTimer m_elapsedTimer;
    bool          m_running;
    int           m_selectedModel;   /* -1=none, 0=PINN, 1=CNN */
    bool          m_converged;
    int           m_fullWindowCheckCount;  /* force-complete counter after window fills */
    float         m_latestSoh;       /* cached for SOH-critical alarm */

    /* ── CNN convergence tracking ── */
    std::deque<int>   m_cnnStageHistory;
    std::deque<float> m_cnnRulHistory;
    int               m_cnnStableStageCount;
    int               m_cnnStableRulCount;
    int               m_cnnFullWindowCount;
};

#endif /* HMI_UI_MAIN_WINDOW_H */
