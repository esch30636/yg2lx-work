/*===========================================================================
 * CycleProgressBar.cpp — Cycle test progress implementation
 *===========================================================================*/
#include "CycleProgressBar.h"
#include "config/AppConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

CycleProgressBar::CycleProgressBar(QWidget *parent)
    : QWidget(parent)
    , m_current(0), m_total(CYCLE_TOTAL_TARGET), m_elapsedSec(0.0)
{
    setupUi();
}

void CycleProgressBar::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(6);

    /* Title */
    m_titleLabel = new QLabel(tr("Cycle Test Progress"));
    m_titleLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(COLOR_TEXT));
    mainLayout->addWidget(m_titleLabel);

    /* Progress bar */
    m_progressBar = new QProgressBar();
    m_progressBar->setObjectName("cycleBar");
    m_progressBar->setRange(0, CYCLE_TOTAL_TARGET);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(tr("%v / %m cycles"));
    m_progressBar->setMinimumHeight(24);
    mainLayout->addWidget(m_progressBar);

    /* Count label */
    m_countLabel = new QLabel("0 / 2000");
    m_countLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(COLOR_TEXT_DIM));
    m_countLabel->setAlignment(Qt::AlignRight);
    mainLayout->addWidget(m_countLabel);

    /* Elapsed time */
    QHBoxLayout *timeRow = new QHBoxLayout();
    m_elapsedLabel = new QLabel(tr("Elapsed: --:--:--"));
    m_elapsedLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(COLOR_ACCENT_CYAN));
    m_remainingLabel = new QLabel(tr("ETA: --:--:--"));
    m_remainingLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(COLOR_TEXT_DIM));
    m_remainingLabel->setAlignment(Qt::AlignRight);
    timeRow->addWidget(m_elapsedLabel);
    timeRow->addWidget(m_remainingLabel, 1);
    mainLayout->addLayout(timeRow);

    /* Status */
    m_statusLabel = new QLabel("IDLE");
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);
}

static QString formatDuration(double totalSec)
{
    int hours   = static_cast<int>(totalSec) / 3600;
    int minutes = (static_cast<int>(totalSec) % 3600) / 60;
    int seconds = static_cast<int>(totalSec) % 60;
    return QString("%1:%2:%3")
        .arg(hours,   2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

void CycleProgressBar::setProgress(uint32_t current, uint32_t total)
{
    m_current = current;
    m_total = total;

    m_progressBar->setMaximum(static_cast<int>(total));
    m_progressBar->setValue(static_cast<int>(current));
    m_countLabel->setText(QString("%1 / %2 cycles").arg(current).arg(total));

    /* Estimate remaining time (linear extrapolation) */
    if (current > 0 && m_elapsedSec > 1.0) {
        double rate = static_cast<double>(current) / m_elapsedSec;  /* cycles/sec */
        double remaining = static_cast<double>(total - current) / rate;
        m_remainingLabel->setText(tr("ETA: %1").arg(formatDuration(remaining)));
    }
}

void CycleProgressBar::setElapsed(double elapsedSec)
{
    m_elapsedSec = elapsedSec;
    m_elapsedLabel->setText(tr("Elapsed: %1").arg(formatDuration(elapsedSec)));
}

void CycleProgressBar::setStatus(const QString &status)
{
    m_statusLabel->setText(status);

    if (status == "RUNNING")
        m_statusLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
    else if (status == "PAUSED")
        m_statusLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(COLOR_WARNING_YELLOW));
    else if (status == "COMPLETED")
        m_statusLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(COLOR_ACCENT_CYAN));
    else
        m_statusLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(COLOR_TEXT_DIM));
}
