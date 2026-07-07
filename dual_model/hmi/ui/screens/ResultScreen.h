/*===========================================================================
 * ResultScreen.h — Screen 3: Unified result display (PINN & CNN)
 *
 * Five display elements (same for both models):
 *   1. IC Curve (dQ/dV vs Voltage) — real-time from lower computer
 *   2. Health prediction value — PINN: SOH, CNN: RUL
 *   3. Confidence — PINN: 95% CI half-width, CNN: stage softmax probability
 *   4. Temperature — direct sensor readout
 *   5. Swelling — bool: Normal / Warning
 *
 * PINN mode: stops automatically when SOH converges (stddev < epsilon)
 * CNN mode:  runs continuously, RUL updated on each inference
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

signals:
    /** User wants to go back to model selection */
    void backClicked();

    /** User requests stop/restart */
    void stopClicked();

private:
    void setupUi();

    Mode m_mode;

    /* Chart */
    ChartWidget *m_chartWidget;

    /* Health section */
    QLabel       *m_healthTitle;
    QLabel       *m_healthValue;
    QLabel       *m_healthConfidence;
    QProgressBar *m_healthBar;

    /* Info row */
    QLabel       *m_tempLabel;
    QLabel       *m_tempValue;
    QLabel       *m_swellLabel;
    QLabel       *m_swellValue;

    /* Status */
    QLabel       *m_statusLabel;

    /* Buttons */
    QPushButton  *m_backButton;
    QPushButton  *m_stopButton;
};

#endif /* HMI_UI_SCREENS_RESULT_SCREEN_H */
