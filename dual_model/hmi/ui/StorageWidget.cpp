/*===========================================================================
 * StorageWidget.cpp — NAND storage monitoring implementation
 *===========================================================================*/
#include "StorageWidget.h"
#include "config/AppConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>

StorageWidget::StorageWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void StorageWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(6);

    /* Title */
    m_titleLabel = new QLabel(tr("Octa-NAND Storage"));
    m_titleLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(COLOR_TEXT));
    mainLayout->addWidget(m_titleLabel);

    /* Progress bar + percentage */
    QHBoxLayout *barRow = new QHBoxLayout();
    m_progressBar = new QProgressBar();
    m_progressBar->setObjectName("storageBar");
    m_progressBar->setRange(0, 100);
    m_progressBar->setTextVisible(false);
    m_progressBar->setMinimumHeight(20);

    m_percentLabel = new QLabel("--%");
    m_percentLabel->setObjectName("value");
    m_percentLabel->setMinimumWidth(50);
    m_percentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    barRow->addWidget(m_progressBar, 1);
    barRow->addWidget(m_percentLabel);
    mainLayout->addLayout(barRow);

    /* Detail: Used / Total / Free */
    m_detailLabel = new QLabel(tr("Used: -- GB / -- GB\nFree: -- GB"));
    m_detailLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(COLOR_TEXT_DIM));
    mainLayout->addWidget(m_detailLabel);

    /* Bad blocks */
    m_badBlockLabel = new QLabel(tr("Bad Blocks: 0 / 4096"));
    m_badBlockLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(COLOR_TEXT_DIM));
    mainLayout->addWidget(m_badBlockLabel);
}

QString StorageWidget::formatBytes(uint64_t bytes)
{
    double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    return QString("%1 GB").arg(gb, 0, 'f', 1);
}

void StorageWidget::setUsage(float percent, uint64_t usedBytes, uint64_t totalBytes,
                              uint32_t badBlocks, uint32_t totalBlocks)
{
    int pct = static_cast<int>(percent);
    m_progressBar->setValue(pct);

    /* Only re-polish when bar colour band actually changes */
    const QString newStyle = (percent >= STORAGE_CRIT_PCT) ? QStringLiteral("criticalBar")
                           : (percent >= STORAGE_WARN_PCT)  ? QStringLiteral("warningBar")
                           :                                  QStringLiteral("storageBar");
    if (newStyle != m_lastStorageBarStyle) {
        m_lastStorageBarStyle = newStyle;
        m_progressBar->setObjectName(newStyle);
        m_progressBar->style()->unpolish(m_progressBar);
        m_progressBar->style()->polish(m_progressBar);
    }

    /* Color-code percentage label */
    if (percent >= STORAGE_CRIT_PCT) {
        m_percentLabel->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_CRITICAL_RED));
    } else if (percent >= STORAGE_WARN_PCT) {
        m_percentLabel->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_WARNING_YELLOW));
    } else {
        m_percentLabel->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_PROGRESS_BLUE));
    }

    m_percentLabel->setText(QString("%1%").arg(pct));

    uint64_t freeBytes = totalBytes - usedBytes;
    m_detailLabel->setText(tr("Used: %1 / %2\nFree: %3")
        .arg(formatBytes(usedBytes))
        .arg(formatBytes(totalBytes))
        .arg(formatBytes(freeBytes)));

    m_badBlockLabel->setText(tr("Bad Blocks: %1 / %2").arg(badBlocks).arg(totalBlocks));
    if (badBlocks > totalBlocks / 10) {
        m_badBlockLabel->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold;").arg(COLOR_CRITICAL_RED));
    } else {
        m_badBlockLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(COLOR_TEXT_DIM));
    }
}
