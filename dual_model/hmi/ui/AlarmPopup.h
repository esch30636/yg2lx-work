/*===========================================================================
 * AlarmPopup.h — Module 4: System alarm popup dialog
 *
 * Frameless modal popup for battery safety alarms:
 *   - Over-temperature (>60°C)
 *   - Over-voltage (>4.25V)
 *   - Over-current (>100A)
 *   - Cell swelling (sensor > threshold)
 *
 * Shows alarm icon, description, current value vs threshold, and
 * an acknowledge button. Auto-dismisses after 10 seconds.
 *===========================================================================*/
#ifndef HMI_UI_ALARM_POPUP_H
#define HMI_UI_ALARM_POPUP_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

class AlarmPopup : public QDialog {
    Q_OBJECT
public:
    enum AlarmId {
        OverTemperature = 0,
        OverVoltage     = 1,
        OverCurrent     = 2,
        CellSwelling    = 3,
        SohCritical     = 4,
        SohWarning      = 5
    };

    /**
     * @param id           Alarm type identifier
     * @param title        Short alarm title (e.g. "Over-Temperature")
     * @param description  Human-readable description
     * @param currentValue Current measured value
     * @param threshold    Alarm threshold
     * @param parent       Parent widget
     */
    AlarmPopup(AlarmId id, const QString &title, const QString &description,
               float currentValue, float threshold, const QString &unit,
               QWidget *parent = nullptr);
    ~AlarmPopup() override;

    AlarmId alarmId() const { return m_alarmId; }

signals:
    void acknowledged(int instanceId);

protected:
    void showEvent(QShowEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onAcknowledge();
    void onAutoDismiss();

private:
    void setupUi(const QString &title, const QString &description,
                 float currentValue, float threshold, const QString &unit);

    AlarmId     m_alarmId;
    int         m_instanceId;
    QTimer     *m_autoDismissTimer;
    QLabel     *m_iconLabel;
    QLabel     *m_titleLabel;
    QLabel     *m_descLabel;
    QLabel     *m_valueLabel;
    QPushButton *m_ackBtn;
};

#endif /* HMI_UI_ALARM_POPUP_H */
