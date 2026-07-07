/*===========================================================================
 * ResultScreen.cpp — Unified result screen implementation
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
    , m_healthTitle(nullptr)
    , m_healthValue(nullptr)
    , m_healthConfidence(nullptr)
    , m_healthBar(nullptr)
    , m_tempLabel(nullptr)
    , m_tempValue(nullptr)
    , m_swellLabel(nullptr)
    , m_swellValue(nullptr)
    , m_statusLabel(nullptr)
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

    /* ── IC Curve chart ── */
    m_chartWidget = new ChartWidget();
    m_chartWidget->setObjectName("panel");
    m_chartWidget->setMinimumHeight(IC_CHART_HEIGHT);
    m_chartWidget->setMaximumHeight(IC_CHART_HEIGHT);
    mainLayout->addWidget(m_chartWidget);

    /* ── Separator ── */
    QFrame *sep1 = new QFrame();
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet(QString("color: %1;").arg(COLOR_BORDER));
    sep1->setFixedHeight(1);
    mainLayout->addWidget(sep1);

    /* ── Health section ── */
    QWidget *healthPanel = new QWidget();
    healthPanel->setObjectName("panel");
    QVBoxLayout *healthLayout = new QVBoxLayout(healthPanel);
    healthLayout->setContentsMargins(12, 8, 12, 8);
    healthLayout->setSpacing(4);

    /* Health header row: title + value + confidence */
    QHBoxLayout *healthHeader = new QHBoxLayout();
    m_healthTitle = new QLabel(tr("SOH"));
    m_healthTitle->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(COLOR_TEXT_DIM));

    m_healthValue = new QLabel("--%");
    m_healthValue->setObjectName("value");
    m_healthValue->setStyleSheet(QString("color: %1; font-size: 28px; font-weight: bold;").arg(COLOR_ACCENT_CYAN));
    m_healthValue->setAlignment(Qt::AlignCenter);

    m_healthConfidence = new QLabel("");
    m_healthConfidence->setStyleSheet(QString("color: %1; font-size: 14px;").arg(COLOR_TEXT_DIM));
    m_healthConfidence->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_healthConfidence->setMinimumWidth(80);

    healthHeader->addWidget(m_healthTitle);
    healthHeader->addWidget(m_healthValue, 1);
    healthHeader->addWidget(m_healthConfidence);
    healthLayout->addLayout(healthHeader);

    /* Health progress bar */
    m_healthBar = new QProgressBar();
    m_healthBar->setRange(0, 100);
    m_healthBar->setTextVisible(false);
    m_healthBar->setMinimumHeight(HEALTH_BAR_HEIGHT);
    m_healthBar->setObjectName("healthyBar");
    healthLayout->addWidget(m_healthBar);

    mainLayout->addWidget(healthPanel);

    /* ── Info row: Temperature + Swelling side by side ── */
    QHBoxLayout *infoRow = new QHBoxLayout();
    infoRow->setSpacing(12);

    /* Temperature */
    QWidget *tempBox = new QWidget();
    tempBox->setObjectName("panel");
    QHBoxLayout *tempLayout = new QHBoxLayout(tempBox);
    tempLayout->setContentsMargins(12, 6, 12, 6);
    m_tempLabel = new QLabel(tr("Temp"));
    m_tempLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(COLOR_TEXT_DIM));
    m_tempValue = new QLabel("-- °C");
    m_tempValue->setObjectName("value");
    m_tempValue->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold;").arg(COLOR_ACCENT_CYAN));
    m_tempValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tempLayout->addWidget(m_tempLabel);
    tempLayout->addWidget(m_tempValue, 1);
    infoRow->addWidget(tempBox, 1);

    /* Swelling */
    QWidget *swellBox = new QWidget();
    swellBox->setObjectName("panel");
    QHBoxLayout *swellLayout = new QHBoxLayout(swellBox);
    swellLayout->setContentsMargins(12, 6, 12, 6);
    m_swellLabel = new QLabel(tr("Swelling"));
    m_swellLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(COLOR_TEXT_DIM));
    m_swellValue = new QLabel("--");
    m_swellValue->setObjectName("value");
    m_swellValue->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
    m_swellValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    swellLayout->addWidget(m_swellLabel);
    swellLayout->addWidget(m_swellValue, 1);
    infoRow->addWidget(swellBox, 1);

    mainLayout->addLayout(infoRow);

    /* ── Status + buttons row ── */
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(8);

    m_statusLabel = new QLabel(tr("Ready"));
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(COLOR_TEXT_DIM));
    bottomRow->addWidget(m_statusLabel, 1);

    m_stopButton = new QPushButton(tr("Stop"));
    m_stopButton->setObjectName("danger");
    m_stopButton->setFixedWidth(80);
    m_stopButton->setVisible(false);  /* hidden until running */
    bottomRow->addWidget(m_stopButton);

    m_backButton = new QPushButton(tr("Back"));
    m_backButton->setFixedWidth(80);
    bottomRow->addWidget(m_backButton);

    mainLayout->addLayout(bottomRow);

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
        m_healthTitle->setText(tr("SOH"));
        m_statusLabel->setText(tr("PINN mode — acquiring..."));
    } else {
        m_healthTitle->setText(tr("RUL"));
        m_statusLabel->setText(tr("CNN mode — acquiring..."));
    }
    m_healthValue->setText("--%");
    m_healthConfidence->setText("");
    m_healthBar->setValue(0);
}

void ResultScreen::setIcCurve(const float ic[128])
{
    m_chartWidget->setIcCurve(ic);
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
        m_healthValue->setStyleSheet(QString("color: %1; font-size: 28px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
        m_healthBar->setObjectName("healthyBar");
    } else if (value > 0.60f) {
        m_healthValue->setStyleSheet(QString("color: %1; font-size: 28px; font-weight: bold;").arg(COLOR_WARNING_YELLOW));
        m_healthBar->setObjectName("warningBar");
    } else {
        m_healthValue->setStyleSheet(QString("color: %1; font-size: 28px; font-weight: bold;").arg(COLOR_CRITICAL_RED));
        m_healthBar->setObjectName("criticalBar");
    }

    /* Re-polish progress bar for color change */
    m_healthBar->style()->unpolish(m_healthBar);
    m_healthBar->style()->polish(m_healthBar);

    /* Confidence display */
    if (m_mode == PINN) {
        int ciPct = static_cast<int>(confidence * 100.0f);
        m_healthConfidence->setText(QString("\302\261%1%").arg(ciPct));  /* ±X% */
    } else {
        int confPct = static_cast<int>(confidence * 100.0f);
        m_healthConfidence->setText(QString("conf: %1%").arg(confPct));
    }
}

void ResultScreen::setTemperature(float tempC)
{
    m_tempValue->setText(QString("%1 °C").arg(tempC, 0, 'f', 1));

    if (tempC > ALARM_TEMP_MAX_C)
        m_tempValue->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold;").arg(COLOR_CRITICAL_RED));
    else if (tempC > ALARM_TEMP_MAX_C * 0.8f)
        m_tempValue->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold;").arg(COLOR_WARNING_YELLOW));
    else
        m_tempValue->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold;").arg(COLOR_ACCENT_CYAN));
}

void ResultScreen::setSwelling(bool swollen)
{
    if (swollen) {
        m_swellValue->setText(tr("WARNING"));
        m_swellValue->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold;").arg(COLOR_CRITICAL_RED));
    } else {
        m_swellValue->setText(tr("Normal"));
        m_swellValue->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
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
        m_statusLabel->setText(tr("✓ Converged — SOH: %1% ±%2%").arg(pct).arg(ciPct));
        m_statusLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;").arg(COLOR_HEALTHY_GREEN));
        m_stopButton->setVisible(true);
        m_stopButton->setText(tr("Done"));
    } else {
        m_statusLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(COLOR_TEXT_DIM));
    }
}
