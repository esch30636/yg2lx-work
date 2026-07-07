/*===========================================================================
 * StatusPanel.h — Module 2: Battery health status parameter display
 *
 * Shows SOH, Stage, RUL, Temperature, Cycle Count, Capacity
 * with color-coded progress bars and formatted labels.
 *===========================================================================*/
#ifndef HMI_UI_STATUS_PANEL_H
#define HMI_UI_STATUS_PANEL_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QTimer>

class StatusPanel : public QWidget {
    Q_OBJECT
public:
    explicit StatusPanel(QWidget *parent = nullptr);

    /* ── Setters (called from MainWindow timer) ── */
    void setSoh(float soh);
    void setSohAccumulated(float median, float ciLow, float ciHigh,
                           int samples, bool windowFull);
    void setStage(int stage, const float probs[3]);
    void setRul(float rul);
    void setTemperature(float tempC);
    void setCycleCount(uint32_t count, uint32_t total);
    void setCapacity(float mah);
    void setSwelling(float swelling);

private:
    void setupUi();
    QString barStyle(float value, float warnThreshold, float critThreshold);
    QString stageColor(int stage);

    /* SOH */
    QLabel       *m_sohLabel;
    QProgressBar *m_sohBar;
    QLabel       *m_sohValue;
    QLabel       *m_sohSub;       /* CI bounds + sample count */

    /* Stage */
    QLabel       *m_stageLabel;
    QLabel       *m_stageValue;
    QLabel       *m_stageDesc;

    /* RUL */
    QLabel       *m_rulLabel;
    QProgressBar *m_rulBar;
    QLabel       *m_rulValue;

    /* Temperature */
    QLabel       *m_tempLabel;
    QLabel       *m_tempValue;

    /* Cycle */
    QLabel       *m_cycleLabel;
    QLabel       *m_cycleValue;

    /* Capacity */
    QLabel       *m_capLabel;
    QLabel       *m_capValue;

    /* Cell Swelling (pressure sensor) */
    QLabel       *m_swellLabel;
    QProgressBar *m_swellBar;
    QLabel       *m_swellValue;

    /* Cache */
    float m_soh;
    int m_stage;
    float m_rul;
    QString m_lastSohBarStyle;   /* avoids redundant GPU re-polish */
    QString m_lastRulBarStyle;
};

#endif /* HMI_UI_STATUS_PANEL_H */
