/*===========================================================================
 * AlarmIndicator.h — Persistent alarm status indicator in header bar
 *
 * A small dot that:
 *   - Blinks red when unacknowledged alarms exist
 *   - Is dim/gray when no alarms
 *   - Clickable to show alarm summary
 *===========================================================================*/
#ifndef HMI_UI_ALARM_INDICATOR_H
#define HMI_UI_ALARM_INDICATOR_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <vector>
#include "AlarmPopup.h"

class AlarmIndicator : public QWidget {
    Q_OBJECT
public:
    explicit AlarmIndicator(QWidget *parent = nullptr);

    /** Show an alarm popup and add to active list */
    void triggerAlarm(AlarmPopup::AlarmId id, const QString &title,
                      const QString &description, float currentValue, float threshold);

    /** Acknowledge and dismiss all alarms */
    void acknowledgeAll();

    /** Number of currently active (unacknowledged) alarms */
    int activeCount() const { return m_activeCount; }

signals:
    void alarmTriggered(AlarmPopup::AlarmId id);
    void allClear();

private slots:
    void onBlinkTimer();
    void onAlarmAcknowledged(int alarmId);

private:
    void updateDotStyle();

    QLabel *m_dotLabel;
    QTimer *m_blinkTimer;
    bool m_blinkState;
    int m_activeCount;
    int m_totalTriggered;  /* for unique alarm instance IDs */
};

#endif /* HMI_UI_ALARM_INDICATOR_H */
