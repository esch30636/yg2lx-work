/*===========================================================================
 * MainWindow.cpp — Central HMI orchestrator implementation (v2.0)
 *===========================================================================*/
#include "MainWindow.h"
#include "config/AppConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QApplication>
#include <QStyle>
#include <QFile>
#include <QDateTime>
#include <cstring>
#include <cmath>
#include <cstdio>

MainWindow::MainWindow(DataProvider *provider, QWidget *parent)
    : QMainWindow(parent)
    , m_dataProvider(provider)
    , m_inferenceEngine(nullptr)
    , m_worker(nullptr)
    , m_workerThread(nullptr)
    , m_stack(nullptr)
    , m_initScreen(nullptr)
    , m_modelSelectScreen(nullptr)
    , m_resultScreen(nullptr)
    , m_headerTitle(nullptr)
    , m_headerClock(nullptr)
    , m_dataTimer(nullptr)
    , m_pinnTimer(nullptr)
    , m_cnnTimer(nullptr)
    , m_clockTimer(nullptr)
    , m_elapsedSec(0.0)
    , m_running(false)
    , m_selectedModel(-1)
    , m_converged(false)
{
    std::memset(&m_latestSample, 0, sizeof(m_latestSample));

    /* Initialize inference engine */
    try {
        m_inferenceEngine = new InferenceEngine();
        printf("[HMI] Inference engine initialized (PINN + CNN)\n");
    } catch (const std::exception &e) {
        qFatal("Failed to initialize inference engine: %s", e.what());
    }

    applyTheme();
    setupUi();
    setupWorkerThread();

    /* Clock timer always runs */
    m_clockTimer = new QTimer(this);
    m_clockTimer->setInterval(CLOCK_UPDATE_MS);
    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::onClockUpdate);
    m_clockTimer->start();
}

MainWindow::~MainWindow()
{
    stopAcquisition();

    if (m_clockTimer) m_clockTimer->stop();

    /* Shut down worker thread */
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }

    delete m_inferenceEngine;
    delete m_worker;
}

void MainWindow::applyTheme()
{
    QFile qss(":/style/industrial.qss");
    if (qss.open(QIODevice::ReadOnly)) {
        qApp->setStyleSheet(qss.readAll());
        qss.close();
    }
}

void MainWindow::setupUi()
{
    /* ── Central widget ── */
    QWidget *central = new QWidget(this);
    QVBoxLayout *mainVBox = new QVBoxLayout(central);
    mainVBox->setContentsMargins(0, 0, 0, 0);
    mainVBox->setSpacing(0);

    /* ── Header bar (compact) ── */
    QWidget *header = new QWidget();
    header->setObjectName("headerBar");
    header->setFixedHeight(HEADER_HEIGHT);

    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 4, 12, 4);
    headerLayout->setSpacing(12);

    m_headerTitle = new QLabel(tr("BATTERY HMI v2.0"));
    m_headerTitle->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;").arg(COLOR_TEXT));
    headerLayout->addWidget(m_headerTitle);

    headerLayout->addStretch();

    m_headerClock = new QLabel("00:00:00");
    m_headerClock->setStyleSheet(QString("color: %1; font-size: 13px;").arg(COLOR_TEXT_DIM));
    headerLayout->addWidget(m_headerClock);

    mainVBox->addWidget(header);

    /* ── QStackedWidget: 3 screens ── */
    m_stack = new QStackedWidget();
    m_stack->setStyleSheet(QString("background-color: %1;").arg(COLOR_BG_MAIN));

    /* Screen 0: Init */
    m_initScreen = new InitScreen();
    connect(m_initScreen, &InitScreen::startTestClicked,
            this, &MainWindow::onStartTest);
    m_stack->addWidget(m_initScreen);  /* index 0 */

    /* Screen 1: Model select */
    m_modelSelectScreen = new ModelSelectScreen();
    connect(m_modelSelectScreen, &ModelSelectScreen::modelSelected,
            this, &MainWindow::onModelSelected);
    m_stack->addWidget(m_modelSelectScreen);  /* index 1 */

    /* Screen 2: Result */
    m_resultScreen = new ResultScreen();
    connect(m_resultScreen, &ResultScreen::backClicked,
            this, &MainWindow::onBackToModelSelect);
    connect(m_resultScreen, &ResultScreen::stopClicked,
            this, &MainWindow::onStopRequested);
    m_stack->addWidget(m_resultScreen);  /* index 2 */

    mainVBox->addWidget(m_stack, 1);

    /* ── Status bar ── */
    QStatusBar *statusBar = new QStatusBar();
    statusBar->setStyleSheet(QString(
        "QStatusBar { background-color: #0a0a1a; color: %1; border-top: 1px solid %2; font-size: 11px; }"
    ).arg(COLOR_TEXT_DIM).arg(COLOR_BORDER));
    statusBar->showMessage(tr("Data: %1").arg(m_dataProvider->name()));
    setStatusBar(statusBar);

    setCentralWidget(central);

    /* Fixed size 640×480 */
    setFixedSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    setWindowTitle(QString("Battery HMI v2.0 — %1").arg(m_dataProvider->name()));

    /* Start on init screen */
    m_stack->setCurrentIndex(0);
}

void MainWindow::setupWorkerThread()
{
    m_workerThread = new QThread(this);
    m_worker = new InferenceWorker();  /* no parent — will be moved to thread */
    m_worker->moveToThread(m_workerThread);

    /* MainWindow signal → worker slot (queued, crosses threads) */
    connect(this, &MainWindow::requestCnnInference,
            m_worker, &InferenceWorker::runCnn,
            Qt::QueuedConnection);

    /* Worker result → main thread */
    connect(m_worker, &InferenceWorker::cnnResultReady,
            this, &MainWindow::onCnnResult,
            Qt::QueuedConnection);

    connect(m_worker, &InferenceWorker::cnnError,
            this, &MainWindow::onCnnError,
            Qt::QueuedConnection);

    /* Clean up worker when thread finishes */
    connect(m_workerThread, &QThread::finished,
            m_worker, &QObject::deleteLater);

    m_workerThread->start();
}

/* ═══════════════════════════════════════════════════════════════════
 * Screen transitions
 * ═══════════════════════════════════════════════════════════════════ */

void MainWindow::onStartTest()
{
    printf("[HMI] Init complete → Model selection\n");
    m_stack->setCurrentIndex(1);
}

void MainWindow::onModelSelected(int model)
{
    m_selectedModel = model;
    printf("[HMI] Model selected: %s\n", model == 0 ? "PINN" : "CNN");

    /* Configure result screen */
    m_resultScreen->setMode(model == 0 ? ResultScreen::PINN : ResultScreen::CNN);

    /* Switch to result screen */
    m_stack->setCurrentIndex(2);

    /* Start data acquisition + inference */
    startAcquisition(model);
}

void MainWindow::onBackToModelSelect()
{
    printf("[HMI] Back to model selection\n");
    stopAcquisition();

    /* Clear result screen */
    m_resultScreen->setHealth(0.0f, 0.0f);
    m_resultScreen->setTemperature(0.0f);
    m_resultScreen->setSwelling(false);
    m_resultScreen->setStatus(tr("Ready"));

    m_stack->setCurrentIndex(1);
}

void MainWindow::onStopRequested()
{
    if (m_converged) {
        /* Already converged — treat as "back" */
        onBackToModelSelect();
    } else {
        printf("[HMI] Stop requested by user\n");
        stopAcquisition();
        m_resultScreen->setStatus(tr("Stopped"));
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Timer management
 * ═══════════════════════════════════════════════════════════════════ */

void MainWindow::startAcquisition(int model)
{
    m_running = true;
    m_converged = false;
    m_sohAccumulator.reset();
    std::memset(&m_latestSample, 0, sizeof(m_latestSample));
    m_elapsedTimer.start();
    m_elapsedSec = 0.0;

    /* ── Data acquisition timer (10 Hz, shared by both modes) ── */
    m_dataTimer = new QTimer(this);
    m_dataTimer->setInterval(DATA_ACQUISITION_MS);
    connect(m_dataTimer, &QTimer::timeout, this, &MainWindow::onDataAcquisition);
    m_dataTimer->start();

    if (model == 0) {
        /* ── PINN mode: 2 Hz SOH inference ── */
        m_pinnTimer = new QTimer(this);
        m_pinnTimer->setInterval(PINN_INFERENCE_MS);
        connect(m_pinnTimer, &QTimer::timeout, this, &MainWindow::onPinnInference);
        m_pinnTimer->start();

        m_resultScreen->setStatus(tr("PINN mode — acquiring SOH..."));
        printf("[HMI] PINN acquisition started (data=100ms, pinn=500ms)\n");
    } else {
        /* ── CNN mode: 2 s inference via worker thread ── */
        m_cnnTimer = new QTimer(this);
        m_cnnTimer->setInterval(CNN_INFERENCE_MS);
        connect(m_cnnTimer, &QTimer::timeout, this, [this]() {
            if (m_running && m_worker->isReady() && m_latestSample.timestamp_ms > 0) {
                QVector<float> ic(128);
                std::memcpy(ic.data(), m_latestSample.ic_curve, 128 * sizeof(float));
                emit requestCnnInference(ic);
            }
        });
        m_cnnTimer->start();

        m_resultScreen->setStatus(tr("CNN mode — acquiring RUL..."));
        printf("[HMI] CNN acquisition started (data=100ms, cnn=2000ms)\n");
    }
}

void MainWindow::stopAcquisition()
{
    m_running = false;

    if (m_dataTimer) {
        m_dataTimer->stop();
        delete m_dataTimer;
        m_dataTimer = nullptr;
    }
    if (m_pinnTimer) {
        m_pinnTimer->stop();
        delete m_pinnTimer;
        m_pinnTimer = nullptr;
    }
    if (m_cnnTimer) {
        m_cnnTimer->stop();
        delete m_cnnTimer;
        m_cnnTimer = nullptr;
    }

    printf("[HMI] Acquisition stopped\n");
}

/* ═══════════════════════════════════════════════════════════════════
 * Data acquisition (shared by both modes)
 * ═══════════════════════════════════════════════════════════════════ */

void MainWindow::onDataAcquisition()
{
    if (!m_running) return;

    BatterySample sample;
    if (!m_dataProvider->read(sample)) return;

    m_latestSample = sample;
    m_elapsedSec = m_elapsedTimer.elapsed() / 1000.0;

    /* Heartbeat log every 1 second */
    static int heartbeatTick = 0;
    if (++heartbeatTick % 10 == 0) {
        printf("[HMI] t=%.1fs  temp=%.1f  volt=%.2f  curr=%.2f  "
               "cycle=%u  swell=%.3f\n",
               m_elapsedSec,
               sample.temperature, sample.voltage, sample.current,
               sample.cycle_count, sample.cell_swelling);
        fflush(stdout);
    }

    /* Update IC curve (always) */
    m_resultScreen->setIcCurve(sample.ic_curve);

    /* Update temperature (every 5th tick = 2 Hz) */
    static int auxTick = 0;
    if (++auxTick % 5 == 0) {
        m_resultScreen->setTemperature(sample.temperature);

        /* Swelling: float → bool (threshold at 0.3) */
        bool swollen = (sample.cell_swelling > ALARM_SWELLING_THRESH);
        m_resultScreen->setSwelling(swollen);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * PINN inference (main thread, fast ~3.5ms)
 * ═══════════════════════════════════════════════════════════════════ */

void MainWindow::onPinnInference()
{
    if (!m_running || m_converged || !m_inferenceEngine->isReady()) return;

    /* Run PINN inference */
    float soh = m_inferenceEngine->predictSOH(m_latestSample.features);

    /* Accumulate */
    m_sohAccumulator.push(soh);
    SohAccumulator::Stats stats = m_sohAccumulator.stats();

    /* Update result screen with accumulated stats */
    float displaySoh = stats.median;
    float ciHalf = stats.ci_95_half;

    m_resultScreen->setHealth(displaySoh, ciHalf);

    /* Show sample count / window status */
    if (!stats.window_full) {
        m_resultScreen->setStatus(tr("Stabilizing... %1/%2 samples")
                                  .arg(stats.sample_count)
                                  .arg(SOH_WINDOW_SIZE));
    }

    /* Check convergence */
    if (stats.window_full) {
        SohAccumulator::ConvergenceStatus cs =
            m_sohAccumulator.checkConvergence(
                CONVERGENCE_EPSILON,
                CONVERGENCE_MIN_SAMPLES,
                CONVERGENCE_STABLE_CHECKS);

        if (cs.converged) {
            m_converged = true;
            m_resultScreen->setConverged(true, cs.final_soh, cs.final_ci_half);

            /* Show final value in header */
            int sohPct = static_cast<int>(cs.final_soh * 100.0f);
            int ciPct  = static_cast<int>(cs.final_ci_half * 100.0f);
            m_headerTitle->setText(tr("BATTERY HMI  —  SOH: %1% ±%2%  ✓")
                                   .arg(sohPct).arg(ciPct));
            m_headerTitle->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;")
                                         .arg(COLOR_HEALTHY_GREEN));

            /* Stop PINN timer (keep data timer running for display) */
            if (m_pinnTimer) {
                m_pinnTimer->stop();
            }

            printf("[HMI] PINN CONVERGED: SOH=%.3f ±%.4f (σ=%.5f, %d samples)\n",
                   cs.final_soh, cs.final_ci_half, cs.current_stddev, cs.samples_used);
        } else if (cs.stable_count > 0) {
            m_resultScreen->setStatus(tr("Converging... σ=%1 (stable: %2/%3)")
                                      .arg(cs.current_stddev, 0, 'f', 4)
                                      .arg(cs.stable_count)
                                      .arg(CONVERGENCE_STABLE_CHECKS));
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * CNN results (from worker thread, via QueuedConnection)
 * ═══════════════════════════════════════════════════════════════════ */

void MainWindow::onCnnResult(int stage, QVector<float> probs, float rul)
{
    if (!m_running) return;

    /* Update result screen health display with RUL */
    m_resultScreen->setHealth(rul, probs[stage]);  /* max stage prob as confidence */

    /* Update status with stage info */
    static const char *stageNames[] = { "HEALTHY", "DEGRADING", "EOL" };
    if (stage >= 0 && stage < 3) {
        m_resultScreen->setStatus(tr("Stage: %1 (RUL: %2%)  |  Data: %3 samples")
                                  .arg(stageNames[stage])
                                  .arg(static_cast<int>(rul * 100.0f))
                                  .arg(m_sohAccumulator.count() + 1));
    }

    /* Track sample count (reuse accumulator counter for display) */
    m_sohAccumulator.push(rul);  /* just for counting */

    printf("[HMI] CNN: stage=%d (%s)  rul=%.3f  probs=[%.3f, %.3f, %.3f]\n",
           stage, stage >= 0 && stage < 3 ? stageNames[stage] : "?",
           rul, probs[0], probs[1], probs[2]);
}

void MainWindow::onCnnError(const QString &msg)
{
    statusBar()->showMessage(tr("CNN Error: %1").arg(msg), 5000);
    printf("[HMI] CNN ERROR: %s\n", qPrintable(msg));
}

void MainWindow::onClockUpdate()
{
    m_headerClock->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_running = false;
    stopAcquisition();

    if (m_clockTimer) m_clockTimer->stop();

    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(2000);
    }

    QMainWindow::closeEvent(event);
}
