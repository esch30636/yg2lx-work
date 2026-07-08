/*===========================================================================
 * ResultScreen.h — Screen 3: Oscilloscope result display (v3.0)
 *
 * 1920×1080 light theme layout:
 *   ┌──────────────────────────────────────────────┐
 *   │  Voltage/Current vs Time Oscilloscope Chart   │  ← ~780px
 *   │  [blue=V trace]  [red=A trace]               │
 *   ├──────────────────────────────────────────────┤
 *   │  Volt: 3.25V  │  Curr: -2.3A  │  Temp: 32°C │  ← digital readout bar
 *   ├──────────────────────────────────────────────┤
 *   │  SOH: ████████░░ 85%  │  Status  │  Back     │  ← bottom bar
 *   └──────────────────────────────────────────────┘
 *
 * PINN mode: stops automatically when SOH converges
 * CNN mode:  runs continuously, RUL updated each inference
 *===========================================================================*/
#ifndef HMI_UI_SCREENS_RESULT_SCREEN_H
#define HMI_UI_SCREENS_RESULT_SCREEN_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include "../ChartWidget.h"

class ResultScreen : public QWidget {
    Q_OBJECT
public:
    enum Mode { PINN = 0, CNN = 1 };

    explicit ResultScreen(QWidget *parent = nullptr);

    /** Set inference mode (PINN or CNN) — updates labels */
    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

public slots:
    /** Update IC curve from lower computer (128-point dQ/dV array) */
    void setIcCurve(const float ic[128]);

    /** Append oscilloscope data point (voltage + current vs time) */
    void appendOscilloscopeData(double timeSec, double voltage, double current);

    /**
     * Update health prediction.
     * PINN: value=SOH [0,1], confidence=95% CI half-width [0,1]
     * CNN:  value=RUL [0,1], confidence=stage softmax max probability [0,1]
     */
    void setHealth(float value, float confidence);

    /** Update temperature display */
    void setTemperature(float tempC);

    /** Update swelling status (true = swelling detected) */
    void setSwelling(bool swollen);

    /** Update status bar text */
    void setStatus(const QString &text);

    /** Mark PINN as converged (shows final value) */
    void setConverged(bool converged, float finalSoh, float finalCiHalf);

    /**
     * Show completion banner overlay (PINN).
     * natural=true → "收敛完成", natural=false → "超时终止"
     */
    void showCompletion(float finalSoh, float finalCiHalf,
                        int samples, float stddev, bool natural);

    /**
     * Show completion banner overlay (CNN).
     * Displays final RUL, stage name, confidence, and inference count.
     */
    void showCnnCompletion(float finalRul, float finalConfidence,
                           int stage, int inferenceCount, bool natural);

    /** Clear all chart series data (call when switching sessions) */
    void clearChart();

signals:
    /** User wants to go back to model selection */
    void backClicked();

    /** User requests stop/restart */
    void stopClicked();

private:
    void setupUi();

    Mode m_mode;

    /* Oscilloscope chart (dominant element) */
    ChartWidget *m_chartWidget;

    /* Digital readout labels */
    QLabel *m_voltReadoutValue;
    QLabel *m_currReadoutValue;
    QLabel *m_tempReadoutValue;

    /* Health section */
    QLabel       *m_healthTitle;
    QLabel       *m_healthValue;
    QLabel       *m_healthConfidence;
    QProgressBar *m_healthBar;

    /* Swelling indicator */
    QLabel       *m_swellValue;

    /* Status */
    QLabel       *m_statusLabel;

    /* Completion banner */
    QWidget      *m_completionBanner;
    QLabel       *m_completionTitle;
    QLabel       *m_completionDetail;

    /* Buttons */
    QPushButton  *m_backButton;
    QPushButton  *m_stopButton;
};

#endif /* HMI_UI_SCREENS_RESULT_SCREEN_H */
