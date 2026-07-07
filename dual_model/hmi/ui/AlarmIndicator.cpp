/*===========================================================================
 * AlarmIndicator.cpp — Alarm indicator implementation
 *===========================================================================*/
#include "AlarmIndicator.h"
#include "config/AppConfig.h"
#include <QHBoxLayout>

AlarmIndicator::AlarmIndicator(QWidget *parent)
    : QWidget(parent)
    , m_dotLabel(nullptr)
    , m_blinkTimer(nullptr)
    , m_blinkState(false)
    , m_activeCount(0)
    , m_totalTriggered(0)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_dotLabel = new QLabel();
    m_dotLabel->setFixedSize(14, 14);
    m_dotLabel->setObjectName("alarmDotInactive");
    m_dotLabel->setStyleSheet(QString(
        "QLabel#alarmDotInactive {"
        "  background-color: %1;"
        "  border-radius: 7px;"
        "}"
    ).arg(COLOR_BORDER));
    layout->addWidget(m_dotLabel);

    /* Blink timer: 500ms interval */
    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(500);
    connect(m_blinkTimer, &QTimer::timeout, this, &AlarmIndicator::onBlinkTimer);
}

void AlarmIndicator::triggerAlarm(AlarmPopup::AlarmId id, const QString &title,
                                   const QString &description,
                                   float currentValue, float threshold)
{
    m_totalTriggered++;
    m_activeCount++;

    /* Create popup */
    AlarmPopup *popup = new AlarmPopup(id, title, description,
                                        currentValue, threshold,
                                        (id == AlarmPopup::OverTemperature) ? "°C" :
                                        (id == AlarmPopup::OverVoltage)     ? "V"  :
                                        (id == AlarmPopup::OverCurrent)     ? "A"  : "%",
                                        parentWidget() ? parentWidget()->window() : nullptr);
    connect(popup, &AlarmPopup::acknowledged, this, &AlarmIndicator::onAlarmAcknowledged);
    popup->show();

    /* Start blinking if this is the first alarm */
    if (m_activeCount == 1) {
        m_blinkTimer->start();
    }

    updateDotStyle();
}

void AlarmIndicator::acknowledgeAll()
{
    m_activeCount = 0;
    m_blinkTimer->stop();
    updateDotStyle();
    emit allClear();
}

void AlarmIndicator::onBlinkTimer()
{
    m_blinkState = !m_blinkState;
    updateDotStyle();
}

void AlarmIndicator::onAlarmAcknowledged(int /*instanceId*/)
{
    m_activeCount--;
    if (m_activeCount <= 0) {
        m_activeCount = 0;
        m_blinkTimer->stop();
        emit allClear();
    }
    updateDotStyle();
}

void AlarmIndicator::updateDotStyle()
{
    if (m_activeCount > 0 && m_blinkState) {
        m_dotLabel->setStyleSheet(QString(
            "QLabel {"
            "  background-color: %1;"
            "  border-radius: 7px;"
            "  border: 1px solid %1;"
            "}"
        ).arg(COLOR_CRITICAL_RED));
    } else {
        m_dotLabel->setStyleSheet(QString(
            "QLabel {"
            "  background-color: %1;"
            "  border-radius: 7px;"
            "}"
        ).arg(COLOR_BORDER));
    }
}
