/*===========================================================================
 * ResultScreen.cpp — Oscilloscope result screen implementation (v3.0)
 *
 * 1920×1080 light theme. Dominant oscilloscope chart (voltage/current vs time)
 * with digital readout bar below it.
 *===========================================================================*/
#include "ResultScreen.h"
#include "config/AppConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QStyle>

ResultScreen::ResultScreen(QWidget *parent)
    : QWidget(parent)
    , m_mode(PINN)
    , m_chartWidget(nullptr)
    , m_voltReadoutValue(nullptr)
    , m_currReadoutValue(nullptr)
    , m_tempReadoutValue(nullptr)
    , m_healthTitle(nullptr)
    , m_healthValue(nullptr)
    , m_healthConfidence(nullptr)
    , m_healthBar(nullptr)
    , m_swellValue(nullptr)
    , m_statusLabel(nullptr)
    , m_completionBanner(nullptr)
    , m_completionTitle(nullptr)
    , m_completionDetail(nullptr)
    , m_backButton(nullptr)
    , m_stopButton(nullptr)
{
    setupUi();
}

void ResultScreen::setupUi()
{
    setStyleSheet(QString("background-color: %1;").arg(COLOR_BG_MAIN));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(4);

    /* ═══════════════════════════════════════════════════════════════
     * 1. Oscilloscope Chart — dominant element (~780px)
     * ═══════════════════════════════════════════════════════════════ */
    m_chartWidget = new ChartWidget();
    m_chartWidget->setObjectName("panel");
    m_chartWidget->setMinimumHeight(CHART_AREA_HEIGHT);
    mainLayout->addWidget(m_chartWidget, 1);  /* stretch factor 1 — takes all space */

    /* ── Thin separator ── */
    QFrame *sep1 = new QFrame();
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet(QString("color: %1;").arg(COLOR_BORDER));
    sep1->setFixedHeight(1);
    mainLayout->addWidget(sep1);

    /* ═══════════════════════════════════════════════════════════════
     * 2. Digital Readout Bar — oscilloscope-style numeric display
     * ═══════════════════════════════════════════════════════════════ */
    QWidget *readoutBar = new QWidget();
    readoutBar->setObjectName("panel");
    readoutBar->setFixedHeight(72);
    QHBoxLayout *readoutLayout = new QHBoxLayout(readoutBar);
    readoutLayout->setContentsMargins(20, 6, 20, 6);
    readoutLayout->setSpacing(40);

    /* ── Voltage readout ── */
    QWidget *voltBox = new QWidget();
    QHBoxLayout *voltLayout = new QHBoxLayout(voltBox);
    voltLayout->setContentsMargins(0, 0, 0, 0);
    voltLayout->setSpacing(12);
    QLabel *voltLabel = new QLabel(tr("电压"));
    voltLabel->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;").arg(COLOR_CHART_VOLTAGE));
    m_voltReadoutValue = new QLabel("-- V");
    m_voltReadoutValue->setObjectName("valueLarge");
    m_voltReadoutValue->setStyleSheet(QString("color: %1; font-size: 36px; font-weight: bold;").arg(COLOR_CHART_VOLTAGE));
    voltLayout->addWidget(voltLabel);
    voltLayout->addWidget(m_voltReadoutValue);
    readoutLayout->addWidget(voltBox);

    /* Vertical separators */
    QFrame *vSep1 = new QFrame();
    vSep1->setFrameShape(QFrame::VLine);
    vSep1->setStyleSheet(QString("color: %1;").arg(COLOR_BORDER));
    readoutLayout->addWidget(vSep1);

    /* ── Current readout ── */
    QWidget *currBox = new QWidget();
    QHBoxLayout *currLayout = new QHBoxLayout(currBox);
    currLayout->setContentsMargins(0, 0, 0, 0);
    currLayout->setSpacing(12);
    QLabel *currLabel = new QLabel(tr("电流"));
    currLabel->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;").arg(COLOR_CHART_CURRENT));
    m_currReadoutValue = new QLabel("-- A");
    m_currReadoutValue->setObjectName("valueLarge");
    m_currReadoutValue->setStyleSheet(QString("color: %1; font-size: 36px; font-weight: bold;").arg(COLOR_CHART_CURRENT));
    currLayout->addWidget(currLabel);
    currLayout->addWidget(m_currReadoutValue);
    readoutLayout->addWidget(currBox);

    QFrame *vSep2 = new QFrame();
    vSep2->setFrameShape(QFrame::VLine);
    vSep2->setStyleSheet(QString("color: %1;").arg(COLOR_BORDER));
    readoutLayout->addWidget(vSep2);

    /* ── Temperature readout ── */
    QWidget *tempBox = new QWidget();
    QHBoxLayout *tempLayout = new QHBoxLayout(tempBox);
    tempLayout->setContentsMargins(0, 0, 0, 0);
    tempLayout->setSpacing(12);
    QLabel *tempLabel = new QLabel(tr("温度"));
    tempLabel->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;").arg(COLOR_TEXT_DIM));
    m_tempReadoutValue = new QLabel("-- °C");
    m_tempReadoutValue->setObjectName("valueLarge");
    m_tempReadoutValue->setStyleSheet(QString("color: %1; font-size: 36px; font-weight: bold;").arg(COLOR_TEXT));
    tempLayout->addWidget(tempLabel);
    tempLayout->addWidget(m_tempReadoutValue);
    readoutLayout->addWidget(tempBox);

    readoutLayout->addStretch();

    /* ── Swelling indicator ── */
    m_swellValue = new QLabel(tr("正常"));
    m_swellValue->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
    readoutLayout->addWidget(m_swellValue);

    mainLayout->addWidget(readoutBar);

    /* ── Completion Banner (hidden until PINN finishes) ── */
    m_completionBanner = new QWidget();
    m_completionBanner->setObjectName("panel");
    m_completionBanner->setFixedHeight(80);
    m_completionBanner->setVisible(false);
    QHBoxLayout *bannerLayout = new QHBoxLayout(m_completionBanner);
    bannerLayout->setContentsMargins(24, 10, 24, 10);
    bannerLayout->setSpacing(24);

    m_completionTitle = new QLabel();
    m_completionTitle->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
    bannerLayout->addWidget(m_completionTitle);

    m_completionDetail = new QLabel();
    m_completionDetail->setStyleSheet(QString("color: %1; font-size: 15px;").arg(COLOR_TEXT));
    bannerLayout->addWidget(m_completionDetail, 1);

    bannerLayout->addStretch();
    mainLayout->addWidget(m_completionBanner);

    /* ── Thin separator ── */
    QFrame *sep2 = new QFrame();
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet(QString("color: %1;").arg(COLOR_BORDER));
    sep2->setFixedHeight(1);
    mainLayout->addWidget(sep2);

    /* ═══════════════════════════════════════════════════════════════
     * 3. Bottom Bar — health + status + buttons
     * ═══════════════════════════════════════════════════════════════ */
    QWidget *bottomBar = new QWidget();
    bottomBar->setObjectName("panel");
    bottomBar->setFixedHeight(56);
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(16, 6, 16, 6);
    bottomLayout->setSpacing(16);

    /* Health title + value */
    m_healthTitle = new QLabel(tr("SOH"));
    m_healthTitle->setStyleSheet(QString("color: %1; font-size: 15px; font-weight: bold;").arg(COLOR_TEXT_DIM));
    bottomLayout->addWidget(m_healthTitle);

    m_healthValue = new QLabel("--%");
    m_healthValue->setStyleSheet(QString("color: %1; font-size: 24px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
    bottomLayout->addWidget(m_healthValue);

    m_healthConfidence = new QLabel("");
    m_healthConfidence->setStyleSheet(QString("color: %1; font-size: 13px;").arg(COLOR_TEXT_DIM));
    m_healthConfidence->setMinimumWidth(130);
    bottomLayout->addWidget(m_healthConfidence);

    /* Health progress bar */
    m_healthBar = new QProgressBar();
    m_healthBar->setRange(0, 100);
    m_healthBar->setTextVisible(false);
    m_healthBar->setMinimumHeight(HEALTH_BAR_HEIGHT);
    m_healthBar->setMaximumHeight(HEALTH_BAR_HEIGHT);
    m_healthBar->setMinimumWidth(200);
    m_healthBar->setMaximumWidth(300);
    m_healthBar->setObjectName("healthyBar");
    bottomLayout->addWidget(m_healthBar);

    bottomLayout->addStretch();

    /* Status text */
    m_statusLabel = new QLabel(tr("就绪"));
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 14px;").arg(COLOR_TEXT_DIM));
    bottomLayout->addWidget(m_statusLabel);

    bottomLayout->addStretch();

    /* Stop button */
    m_stopButton = new QPushButton(tr("停止"));
    m_stopButton->setObjectName("danger");
    m_stopButton->setFixedWidth(100);
    m_stopButton->setVisible(false);
    bottomLayout->addWidget(m_stopButton);

    /* Back button */
    m_backButton = new QPushButton(tr("返回"));
    m_backButton->setFixedWidth(100);
    bottomLayout->addWidget(m_backButton);

    mainLayout->addWidget(bottomBar);

    /* ── Connections ── */
    connect(m_backButton, &QPushButton::clicked,
            this, &ResultScreen::backClicked);
    connect(m_stopButton, &QPushButton::clicked,
            this, &ResultScreen::stopClicked);
}

void ResultScreen::setMode(Mode mode)
{
    m_mode = mode;
    if (mode == PINN) {
        m_healthTitle->setText(tr("健康度"));
        m_statusLabel->setText(tr("PINN 模式 — 采集中..."));
    } else {
        m_healthTitle->setText(tr("健康度"));
        m_statusLabel->setText(tr("CNN 模式 — 采集中..."));
    }
    m_healthValue->setText("--%");
    m_healthConfidence->setText("");
    m_healthBar->setValue(0);
    if (m_completionBanner)
        m_completionBanner->setVisible(false);
    m_stopButton->setVisible(false);
}

void ResultScreen::setIcCurve(const float ic[128])
{
    m_chartWidget->setIcCurve(ic);
}

void ResultScreen::appendOscilloscopeData(double timeSec, double voltage, double current)
{
    m_chartWidget->appendData(timeSec, voltage, current);

    /* Update digital readout */
    m_voltReadoutValue->setText(QString("%1 V").arg(voltage, 0, 'f', 2));
    m_currReadoutValue->setText(QString("%1 A").arg(current, 0, 'f', 2));

    /* Color current based on charge/discharge */
    if (current > 0.05) {
        /* Charging — orange */
        m_currReadoutValue->setStyleSheet(QString("color: %1; font-size: 36px; font-weight: bold;").arg(COLOR_ACCENT_ORANGE));
    } else if (current < -0.05) {
        /* Discharging — blue */
        m_currReadoutValue->setStyleSheet(QString("color: %1; font-size: 36px; font-weight: bold;").arg(COLOR_ACCENT_CYAN));
    } else {
        /* Idle — neutral */
        m_currReadoutValue->setStyleSheet(QString("color: %1; font-size: 36px; font-weight: bold;").arg(COLOR_TEXT));
    }
}

void ResultScreen::setHealth(float value, float confidence)
{
    int pct = static_cast<int>(value * 100.0f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    m_healthValue->setText(QString("%1%").arg(pct));
    m_healthBar->setValue(pct);

    /* Color based on health level */
    if (value > 0.80f) {
        m_healthValue->setStyleSheet(QString("color: %1; font-size: 24px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
        m_healthBar->setObjectName("healthyBar");
    } else if (value > 0.60f) {
        m_healthValue->setStyleSheet(QString("color: %1; font-size: 24px; font-weight: bold;").arg(COLOR_WARNING_YELLOW));
        m_healthBar->setObjectName("warningBar");
    } else {
        m_healthValue->setStyleSheet(QString("color: %1; font-size: 24px; font-weight: bold;").arg(COLOR_CRITICAL_RED));
        m_healthBar->setObjectName("criticalBar");
    }

    /* Re-polish progress bar for color change */
    m_healthBar->style()->unpolish(m_healthBar);
    m_healthBar->style()->polish(m_healthBar);

    /* Confidence display — Chinese label */
    if (m_mode == PINN) {
        int ciPct = static_cast<int>(confidence * 100.0f);
        m_healthConfidence->setText(QString("置信度 \302\261%1%").arg(ciPct));
    } else {
        int confPct = static_cast<int>(confidence * 100.0f);
        m_healthConfidence->setText(QString("置信度 %1%").arg(confPct));
    }
}

void ResultScreen::setTemperature(float tempC)
{
    m_tempReadoutValue->setText(QString("%1 °C").arg(tempC, 0, 'f', 1));

    if (tempC > ALARM_TEMP_MAX_C)
        m_tempReadoutValue->setStyleSheet(QString("color: %1; font-size: 36px; font-weight: bold;").arg(COLOR_CRITICAL_RED));
    else if (tempC > ALARM_TEMP_MAX_C * 0.8f)
        m_tempReadoutValue->setStyleSheet(QString("color: %1; font-size: 36px; font-weight: bold;").arg(COLOR_WARNING_YELLOW));
    else
        m_tempReadoutValue->setStyleSheet(QString("color: %1; font-size: 36px; font-weight: bold;").arg(COLOR_TEXT));
}

void ResultScreen::setSwelling(bool swollen)
{
    if (swollen) {
        m_swellValue->setText(tr("膨胀警告"));
        m_swellValue->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;").arg(COLOR_CRITICAL_RED));
    } else {
        m_swellValue->setText(tr("正常"));
        m_swellValue->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
    }
}

void ResultScreen::setStatus(const QString &text)
{
    m_statusLabel->setText(text);
}

void ResultScreen::setConverged(bool converged, float finalSoh, float finalCiHalf)
{
    if (converged) {
        int pct = static_cast<int>(finalSoh * 100.0f);
        int ciPct = static_cast<int>(finalCiHalf * 100.0f);
        m_statusLabel->setText(tr("✓ 已收敛 — 健康度: %1% ±%2%").arg(pct).arg(ciPct));
        m_statusLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
        m_stopButton->setVisible(true);
        m_stopButton->setText(tr("完成"));
    } else {
        m_statusLabel->setStyleSheet(QString("color: %1; font-size: 14px;").arg(COLOR_TEXT_DIM));
    }
}

void ResultScreen::showCompletion(float finalSoh, float finalCiHalf,
                                    int samples, float /*stddev*/, bool natural)
{
    m_completionBanner->setVisible(true);

    int sohPct = static_cast<int>(finalSoh * 100.0f);
    int ciPct  = static_cast<int>(finalCiHalf * 100.0f);

    if (natural) {
        m_completionTitle->setText(tr("✓ 收敛完成"));
        m_completionTitle->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;")
                                         .arg(COLOR_HEALTHY_GREEN));
    } else {
        m_completionTitle->setText(tr("⏱ 检测完成 (超时终止)"));
        m_completionTitle->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;")
                                         .arg(COLOR_ACCENT_CYAN));
    }

    m_completionDetail->setText(tr("健康度: %1% ±%2%  |  样本数: %3")
                                .arg(sohPct).arg(ciPct).arg(samples));

    /* Also update the status bar and show stop/completion button */
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;")
                                 .arg(natural ? COLOR_HEALTHY_GREEN : COLOR_ACCENT_CYAN));
    m_stopButton->setVisible(true);
    m_stopButton->setText(tr("完成"));
}

void ResultScreen::showCnnCompletion(float finalRul, float finalConfidence,
                                       int stage, int inferenceCount, bool natural)
{
    m_completionBanner->setVisible(true);

    int rulPct  = static_cast<int>(finalRul * 100.0f);
    int confPct = static_cast<int>(finalConfidence * 100.0f);

    static const char *stageNames[] = { "健康", "退化", "寿命终止" };
    const char *stageName = (stage >= 0 && stage < 3) ? stageNames[stage] : "?";

    if (natural) {
        m_completionTitle->setText(tr("✓ 收敛完成"));
        m_completionTitle->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;")
                                         .arg(COLOR_HEALTHY_GREEN));
    } else {
        m_completionTitle->setText(tr("⏱ 检测完成 (超时终止)"));
        m_completionTitle->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: bold;")
                                         .arg(COLOR_ACCENT_CYAN));
    }

    m_completionDetail->setText(tr("健康度: %1%  置信度: %2%  |  阶段: %3  |  推理: %4 次")
                                .arg(rulPct).arg(confPct).arg(stageName).arg(inferenceCount));

    /* Also update status bar */
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;")
                                 .arg(natural ? COLOR_HEALTHY_GREEN : COLOR_ACCENT_CYAN));
    m_stopButton->setVisible(true);
    m_stopButton->setText(tr("完成"));
}

void ResultScreen::clearChart()
{
    m_chartWidget->clear();
    m_voltReadoutValue->setText("-- V");
    m_currReadoutValue->setText("-- A");
    m_tempReadoutValue->setText("-- °C");
    if (m_completionBanner)
        m_completionBanner->setVisible(false);
    m_stopButton->setVisible(false);
}
