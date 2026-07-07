/*===========================================================================
 * InitScreen.cpp — Init screen implementation
 *===========================================================================*/
#include "InitScreen.h"
#include "config/AppConfig.h"
#include <QVBoxLayout>

InitScreen::InitScreen(QWidget *parent)
    : QWidget(parent)
    , m_titleLabel(nullptr)
    , m_progressBar(nullptr)
    , m_startButton(nullptr)
    , m_progressTimer(nullptr)
    , m_progressValue(0)
{
    setupUi();
}

void InitScreen::setupUi()
{
    /* Full-widget background */
    setStyleSheet(QString("background-color: %1;").arg(COLOR_BG_MAIN));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(60, 80, 60, 80);
    layout->setSpacing(24);

    /* ── Title ── */
    m_titleLabel = new QLabel(tr("Device Initializing..."));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(QString(
        "color: %1; font-size: 22px; font-weight: bold;"
    ).arg(COLOR_TEXT));
    layout->addWidget(m_titleLabel);

    layout->addSpacing(20);

    /* ── Progress bar ── */
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    m_progressBar->setMinimumHeight(28);
    m_progressBar->setObjectName("healthyBar");  /* green gradient from QSS */
    layout->addWidget(m_progressBar);

    layout->addSpacing(30);

    /* ── Start button (initially disabled) ── */
    m_startButton = new QPushButton(tr("Start Test"));
    m_startButton->setObjectName("primary");
    m_startButton->setEnabled(false);
    m_startButton->setMinimumHeight(44);
    m_startButton->setStyleSheet(QString(
        "QPushButton#primary { font-size: 18px; padding: 10px 32px; }"
    ));
    layout->addWidget(m_startButton, 0, Qt::AlignCenter);

    layout->addStretch();

    /* ── Connect ── */
    connect(m_startButton, &QPushButton::clicked,
            this, &InitScreen::startTestClicked);

    /* ── Progress animation timer ── */
    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(INIT_PROGRESS_MS);
    connect(m_progressTimer, &QTimer::timeout,
            this, &InitScreen::onProgressTick);
    m_progressTimer->start();
}

void InitScreen::onProgressTick()
{
    /* Advance progress bar over ~3 seconds */
    int totalTicks = INIT_PROGRESS_DURATION_MS / INIT_PROGRESS_MS;  /* 100 ticks */
    m_progressValue++;

    if (m_progressValue >= totalTicks) {
        m_progressValue = 100;
        m_progressBar->setValue(100);
        m_progressTimer->stop();

        /* Enable the start button */
        m_startButton->setEnabled(true);
        m_titleLabel->setText(tr("Device Ready"));
        m_titleLabel->setStyleSheet(QString(
            "color: %1; font-size: 22px; font-weight: bold;"
        ).arg(COLOR_HEALTHY_GREEN));
    } else {
        int pct = (m_progressValue * 100) / totalTicks;
        m_progressBar->setValue(pct);
    }
}
