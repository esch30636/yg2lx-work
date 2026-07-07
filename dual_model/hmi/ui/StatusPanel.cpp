/*===========================================================================
 * StatusPanel.cpp — Battery health status panel implementation
 *===========================================================================*/
#include "StatusPanel.h"
#include "config/AppConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFont>
#include <QStyle>

StatusPanel::StatusPanel(QWidget *parent)
    : QWidget(parent)
    , m_soh(0.0f), m_stage(-1), m_rul(0.0f)
{
    setupUi();
}

void StatusPanel::setupUi()
{
    /* Main layout */
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    /* Section title */
    QLabel *secTitle = new QLabel(tr("Battery Status"));
    secTitle->setObjectName("title");
    secTitle->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;").arg(COLOR_TEXT));
    mainLayout->addWidget(secTitle);

    /* Separator */
    QFrame *sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color: %1;").arg(COLOR_BORDER));
    mainLayout->addWidget(sep);

    /* ── SOH ── */
    {
        QHBoxLayout *row = new QHBoxLayout();
        m_sohLabel = new QLabel(tr("SOH"));
        m_sohLabel->setMinimumWidth(60);
        m_sohLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(COLOR_TEXT_DIM));

        m_sohBar = new QProgressBar();
        m_sohBar->setRange(0, 100);
        m_sohBar->setTextVisible(false);
        m_sohBar->setMinimumHeight(PROGRESS_BAR_HEIGHT);

        m_sohValue = new QLabel("--%");
        m_sohValue->setObjectName("value");
        m_sohValue->setMinimumWidth(60);
        m_sohValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        row->addWidget(m_sohLabel);
        row->addWidget(m_sohBar, 1);
        row->addWidget(m_sohValue);
        mainLayout->addLayout(row);

        /* Sub-label for CI bounds / stabilizing indicator */
        m_sohSub = new QLabel("");
        m_sohSub->setStyleSheet(QString("color: %1; font-size: 10px;").arg(COLOR_TEXT_DIM));
        m_sohSub->setAlignment(Qt::AlignRight);
        mainLayout->addWidget(m_sohSub);
    }

    /* ── Stage ── */
    {
        QHBoxLayout *row = new QHBoxLayout();
        m_stageLabel = new QLabel(tr("Stage"));
        m_stageLabel->setMinimumWidth(60);
        m_stageLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(COLOR_TEXT_DIM));

        m_stageValue = new QLabel("--");
        m_stageValue->setObjectName("value");
        m_stageValue->setStyleSheet(QString("font-size: 16px;"));

        m_stageDesc = new QLabel("");
        m_stageDesc->setStyleSheet(QString("color: %1; font-size: 11px;").arg(COLOR_TEXT_DIM));
        m_stageDesc->setAlignment(Qt::AlignRight);

        row->addWidget(m_stageLabel);
        row->addWidget(m_stageValue);
        row->addWidget(m_stageDesc, 1);
        mainLayout->addLayout(row);
    }

    /* ── RUL ── */
    {
        QHBoxLayout *row = new QHBoxLayout();
        m_rulLabel = new QLabel(tr("RUL"));
        m_rulLabel->setMinimumWidth(60);
        m_rulLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(COLOR_TEXT_DIM));

        m_rulBar = new QProgressBar();
        m_rulBar->setRange(0, 100);
        m_rulBar->setTextVisible(false);
        m_rulBar->setMinimumHeight(PROGRESS_BAR_HEIGHT);

        m_rulValue = new QLabel("--%");
        m_rulValue->setObjectName("value");
        m_rulValue->setMinimumWidth(60);
        m_rulValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        row->addWidget(m_rulLabel);
        row->addWidget(m_rulBar, 1);
        row->addWidget(m_rulValue);
        mainLayout->addLayout(row);
    }

    /* Separator */
    QFrame *sep2 = new QFrame();
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet(QString("color: %1;").arg(COLOR_BORDER));
    mainLayout->addWidget(sep2);

    /* ── Temperature, Cycle, Capacity (form layout) ── */
    QFormLayout *form = new QFormLayout();
    form->setSpacing(6);

    m_tempLabel = new QLabel(tr("Temp"));
    m_tempValue = new QLabel("-- °C");
    m_tempValue->setObjectName("value");
    m_tempValue->setAlignment(Qt::AlignRight);
    form->addRow(m_tempLabel, m_tempValue);

    m_cycleLabel = new QLabel(tr("Cycle"));
    m_cycleValue = new QLabel("--");
    m_cycleValue->setObjectName("value");
    m_cycleValue->setAlignment(Qt::AlignRight);
    form->addRow(m_cycleLabel, m_cycleValue);

    m_capLabel = new QLabel(tr("Capacity"));
    m_capValue = new QLabel("-- mAh");
    m_capValue->setObjectName("value");
    m_capValue->setAlignment(Qt::AlignRight);
    form->addRow(m_capLabel, m_capValue);

    /* Cell Swelling (pressure sensor) */
    m_swellLabel = new QLabel(tr("Pressure"));
    m_swellValue = new QLabel("--");
    m_swellValue->setObjectName("value");
    m_swellValue->setAlignment(Qt::AlignRight);
    form->addRow(m_swellLabel, m_swellValue);

    /* Style form labels */
    for (int i = 0; i < form->rowCount(); i++) {
        QLayoutItem *item = form->itemAt(i, QFormLayout::LabelRole);
        if (item && item->widget()) {
            item->widget()->setStyleSheet(QString("color: %1; font-size: 13px;").arg(COLOR_TEXT_DIM));
        }
    }

    mainLayout->addLayout(form);
    mainLayout->addStretch();
}

QString StatusPanel::barStyle(float value, float warnThreshold, float critThreshold)
{
    if (value <= critThreshold)  return "criticalBar";
    if (value <= warnThreshold)  return "warningBar";
    return "healthyBar";
}

QString StatusPanel::stageColor(int stage)
{
    switch (stage) {
        case 0: return QString("color: %1;").arg(COLOR_HEALTHY_GREEN);
        case 1: return QString("color: %1;").arg(COLOR_WARNING_YELLOW);
        case 2: return QString("color: %1;").arg(COLOR_CRITICAL_RED);
        default: return QString("color: %1;").arg(COLOR_TEXT_DIM);
    }
}

void StatusPanel::setSoh(float soh)
{
    m_soh = soh;
    int pct = static_cast<int>(soh * 100.0f);

    m_sohBar->setValue(pct);

    /* Only re-polish when bar colour band actually changes (avoids expensive
     * GPU style recomputation on embedded EGLFS/Mali every 500ms) */
    const QString newStyle = barStyle(soh, 0.40f, 0.20f);
    if (newStyle != m_lastSohBarStyle) {
        m_lastSohBarStyle = newStyle;
        m_sohBar->setObjectName(newStyle);
        m_sohBar->style()->unpolish(m_sohBar);
        m_sohBar->style()->polish(m_sohBar);
    }

    m_sohValue->setText(QString("%1%").arg(pct));

    if (soh > 0.80f)
        m_sohValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
    else if (soh > 0.60f)
        m_sohValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_WARNING_YELLOW));
    else
        m_sohValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_CRITICAL_RED));

    /* Clear any accumulated sub-label */
    m_sohSub->setText("");
}

void StatusPanel::setSohAccumulated(float median, float ciLow, float ciHigh,
                                     int samples, bool windowFull)
{
    m_soh = median;
    int pct = static_cast<int>(median * 100.0f);

    m_sohBar->setValue(pct);

    /* Only re-polish when bar colour band actually changes */
    const QString newStyle = barStyle(median, 0.40f, 0.20f);
    if (newStyle != m_lastSohBarStyle) {
        m_lastSohBarStyle = newStyle;
        m_sohBar->setObjectName(newStyle);
        m_sohBar->style()->unpolish(m_sohBar);
        m_sohBar->style()->polish(m_sohBar);
    }

    /* Primary value: "82.4%" */
    m_sohValue->setText(QString("%1%").arg(pct));

    if (median > 0.80f)
        m_sohValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
    else if (median > 0.60f)
        m_sohValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_WARNING_YELLOW));
    else
        m_sohValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_CRITICAL_RED));

    /* Sub-label: CI bounds or stabilizing indicator */
    if (!windowFull) {
        m_sohSub->setText(tr("Stabilizing… %1/%2 samples")
                          .arg(samples).arg(SOH_WINDOW_SIZE));
    } else if (ciHigh - ciLow > 0.0001f) {
        /* Only show CI when bounds differ meaningfully */
        m_sohSub->setText(QString("[%1 – %2]  n=%3")
                          .arg(static_cast<int>(ciLow * 100.0f))
                          .arg(static_cast<int>(ciHigh * 100.0f))
                          .arg(samples));
    } else {
        /* Degenerate case: all values identical */
        m_sohSub->setText(QString("n=%1  σ=0").arg(samples));
    }
}

void StatusPanel::setStage(int stage, const float probs[3])
{
    m_stage = stage;
    static const char *names[] = {
        QT_TRANSLATE_NOOP("StatusPanel", "HEALTHY"),
        QT_TRANSLATE_NOOP("StatusPanel", "DEGRADING"),
        QT_TRANSLATE_NOOP("StatusPanel", "EOL")
    };
    static const char *descs[] = {
        QT_TRANSLATE_NOOP("StatusPanel", "Battery in good condition"),
        QT_TRANSLATE_NOOP("StatusPanel", "Performance declining"),
        QT_TRANSLATE_NOOP("StatusPanel", "End of life reached")
    };

    if (stage >= 0 && stage < 3) {
        m_stageValue->setText(tr(names[stage]));
        m_stageValue->setStyleSheet(stageColor(stage) + " font-size: 16px; font-weight: bold;");
        m_stageDesc->setText(QString("%1 (%2%)")
            .arg(tr(descs[stage]))
            .arg(static_cast<int>(probs[stage] * 100.0f)));
    } else {
        m_stageValue->setText(tr("UNKNOWN"));
        m_stageValue->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;").arg(COLOR_TEXT_DIM));
        m_stageDesc->setText(tr("No assessment yet"));
    }
}

void StatusPanel::setRul(float rul)
{
    m_rul = rul;
    int pct = static_cast<int>(rul * 100.0f);

    m_rulBar->setValue(pct);

    /* Only re-polish when bar colour band actually changes */
    const QString newStyle = barStyle(rul, 0.40f, 0.20f);
    if (newStyle != m_lastRulBarStyle) {
        m_lastRulBarStyle = newStyle;
        m_rulBar->setObjectName(newStyle);
        m_rulBar->style()->unpolish(m_rulBar);
        m_rulBar->style()->polish(m_rulBar);
    }

    m_rulValue->setText(QString("%1%").arg(pct));
}

void StatusPanel::setTemperature(float tempC)
{
    m_tempValue->setText(QString("%1 °C").arg(tempC, 0, 'f', 1));
    if (tempC > ALARM_TEMP_MAX_C)
        m_tempValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_CRITICAL_RED));
    else if (tempC > ALARM_TEMP_MAX_C * 0.8f)
        m_tempValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_WARNING_YELLOW));
    else
        m_tempValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_ACCENT_CYAN));
}

void StatusPanel::setCycleCount(uint32_t count, uint32_t total)
{
    m_cycleValue->setText(QString("%1 / %2").arg(count).arg(total));
}

void StatusPanel::setCapacity(float mah)
{
    m_capValue->setText(QString("%1 mAh").arg(mah, 0, 'f', 0));
}

void StatusPanel::setSwelling(float swelling)
{
    /* swelling: 0.0 = normal, 1.0 = severe deformation
     * Display as percentage with color-coded status */
    int pct = static_cast<int>(swelling * 100.0f);

    if (swelling < 0.001f) {
        m_swellValue->setText(tr("Normal"));
        m_swellValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
    } else if (swelling < ALARM_SWELLING_THRESH) {
        m_swellValue->setText(QString("%1%").arg(pct));
        m_swellValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_WARNING_YELLOW));
    } else {
        m_swellValue->setText(QString("⚠ %1%").arg(pct));
        m_swellValue->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_CRITICAL_RED));
    }
}
