/*===========================================================================
 * ChartWidget.cpp — Real-time chart implementation (dual-mode)
 *===========================================================================*/
#include "ChartWidget.h"
#include "config/AppConfig.h"
#include <QtCharts/QChart>
#include <QHBoxLayout>

using namespace QtCharts;

ChartWidget::ChartWidget(QWidget *parent)
    : QWidget(parent)
    , m_chart(nullptr)
    , m_chartView(nullptr)
    , m_voltageSeries(nullptr)
    , m_currentSeries(nullptr)
    , m_axisY_voltage(nullptr)
    , m_axisY_current(nullptr)
    , m_axisX(nullptr)
    , m_icSeries(nullptr)
    , m_axisY_ic(nullptr)
    , m_axisX_ic(nullptr)
    , m_mode(VoltageCurrent)
    , m_timeOffset(0.0)
    , m_lastTime(0.0)
{
    setupChart();
}

ChartWidget::~ChartWidget() = default;

void ChartWidget::setupChart()
{
    /* ── Chart ── */
    m_chart = new QChart();
    m_chart->setTitleBrush(QColor(COLOR_TEXT_DIM));
    m_chart->setBackgroundBrush(QColor(COLOR_BG_PANEL));
    m_chart->setBackgroundRoundness(6);
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setLabelColor(QColor(COLOR_TEXT_DIM));
    m_chart->legend()->setAlignment(Qt::AlignTop);
    m_chart->setAnimationOptions(QChart::NoAnimation);  /* critical for real-time perf */
    m_chart->setMargins(QMargins(4, 4, 4, 4));

    /* ── Chart View ── */
    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing, false);
    /* NOTE: Antialiasing disabled for embedded Mali-G31 — prevents GPU
     * driver state corruption on EGLFS after sustained rendering */
    m_chartView->setBackgroundBrush(QColor(COLOR_BG_PANEL));

    /* Layout */
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);

    /* Start in IC curve mode (primary mode for new UI) */
    setMode(IcCurve);
}

void ChartWidget::setMode(Mode mode)
{
    if (m_mode == mode && m_icSeries != nullptr)
        return;

    m_mode = mode;

    /* Remove all series and axes */
    m_chart->removeAllSeries();
    /* Remove all axes (removeAxis needs the pointer) */
    const auto axes = m_chart->axes();
    for (auto *axis : axes)
        m_chart->removeAxis(axis);

    /* Hide legend in IC mode (single series) */
    m_chart->legend()->setVisible(mode == VoltageCurrent);

    if (mode == VoltageCurrent) {
        setupVoltageCurrentMode();
    } else {
        setupIcCurveMode();
    }
}

void ChartWidget::setupVoltageCurrentMode()
{
    m_chart->setTitle(tr("Charge / Discharge Curves"));

    /* ── Series ── */
    m_voltageSeries = new QLineSeries(this);
    m_voltageSeries->setName(tr("Voltage (V)"));
    m_voltageSeries->setColor(QColor(COLOR_ACCENT_CYAN));
    m_voltageSeries->setPen(QPen(QColor(COLOR_ACCENT_CYAN), 2.0));

    m_currentSeries = new QLineSeries(this);
    m_currentSeries->setName(tr("Current (A)"));
    m_currentSeries->setColor(QColor(COLOR_ACCENT_ORANGE));
    m_currentSeries->setPen(QPen(QColor(COLOR_ACCENT_ORANGE), 2.0));

    m_chart->addSeries(m_voltageSeries);
    m_chart->addSeries(m_currentSeries);

    /* ── Y-axis: Voltage (left) ── */
    m_axisY_voltage = new QValueAxis(this);
    m_axisY_voltage->setTitleText(tr("Voltage (V)"));
    m_axisY_voltage->setTitleBrush(QColor(COLOR_ACCENT_CYAN));
    m_axisY_voltage->setLabelsColor(QColor(COLOR_ACCENT_CYAN));
    m_axisY_voltage->setGridLineColor(QColor("#2a2a4a"));
    m_axisY_voltage->setLinePenColor(QColor(COLOR_ACCENT_CYAN));
    m_axisY_voltage->setRange(0.0, 5.0);
    m_axisY_voltage->setTickCount(6);
    m_chart->addAxis(m_axisY_voltage, Qt::AlignLeft);
    m_voltageSeries->attachAxis(m_axisY_voltage);

    /* ── Y-axis: Current (right) ── */
    m_axisY_current = new QValueAxis(this);
    m_axisY_current->setTitleText(tr("Current (A)"));
    m_axisY_current->setTitleBrush(QColor(COLOR_ACCENT_ORANGE));
    m_axisY_current->setLabelsColor(QColor(COLOR_ACCENT_ORANGE));
    m_axisY_current->setGridLineVisible(false);
    m_axisY_current->setLinePenColor(QColor(COLOR_ACCENT_ORANGE));
    m_axisY_current->setRange(-150.0, 150.0);
    m_axisY_current->setTickCount(7);
    m_chart->addAxis(m_axisY_current, Qt::AlignRight);
    m_currentSeries->attachAxis(m_axisY_current);

    /* ── X-axis: Time ── */
    m_axisX = new QValueAxis(this);
    m_axisX->setTitleText(tr("Time (s)"));
    m_axisX->setTitleBrush(QColor(COLOR_TEXT_DIM));
    m_axisX->setLabelsColor(QColor(COLOR_TEXT_DIM));
    m_axisX->setGridLineColor(QColor("#2a2a4a"));
    m_axisX->setRange(0.0, static_cast<double>(CHART_WINDOW_SECONDS));
    m_axisX->setTickCount(6);
    m_axisX->setLabelFormat("%.0f");
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_voltageSeries->attachAxis(m_axisX);
    m_currentSeries->attachAxis(m_axisX);
}

void ChartWidget::setupIcCurveMode()
{
    m_chart->setTitle(tr("IC Curve (dQ/dV)"));

    /* ── IC series ── */
    m_icSeries = new QLineSeries(this);
    m_icSeries->setName(tr("dQ/dV"));
    m_icSeries->setColor(QColor(COLOR_ACCENT_CYAN));
    m_icSeries->setPen(QPen(QColor(COLOR_ACCENT_CYAN), 2.0));
    m_chart->addSeries(m_icSeries);

    /* ── Y-axis: dQ/dV ── */
    m_axisY_ic = new QValueAxis(this);
    m_axisY_ic->setTitleText(tr("dQ/dV"));
    m_axisY_ic->setTitleBrush(QColor(COLOR_ACCENT_CYAN));
    m_axisY_ic->setLabelsColor(QColor(COLOR_ACCENT_CYAN));
    m_axisY_ic->setGridLineColor(QColor("#2a2a4a"));
    m_axisY_ic->setLinePenColor(QColor(COLOR_ACCENT_CYAN));
    m_axisY_ic->setTickCount(5);
    m_chart->addAxis(m_axisY_ic, Qt::AlignLeft);
    m_icSeries->attachAxis(m_axisY_ic);

    /* ── X-axis: Voltage (fixed 2.5–3.65V for 128-point IC curve) ── */
    m_axisX_ic = new QValueAxis(this);
    m_axisX_ic->setTitleText(tr("Voltage (V)"));
    m_axisX_ic->setTitleBrush(QColor(COLOR_TEXT_DIM));
    m_axisX_ic->setLabelsColor(QColor(COLOR_TEXT_DIM));
    m_axisX_ic->setGridLineColor(QColor("#2a2a4a"));
    m_axisX_ic->setRange(2.5, 3.65);
    m_axisX_ic->setTickCount(6);
    m_axisX_ic->setLabelFormat("%.2f");
    m_chart->addAxis(m_axisX_ic, Qt::AlignBottom);
    m_icSeries->attachAxis(m_axisX_ic);
}

void ChartWidget::setIcCurve(const float ic[128])
{
    if (m_mode != IcCurve)
        setMode(IcCurve);

    if (!m_icSeries)
        return;

    /* Full replace: 128 points on voltage grid [2.5, 3.65] */
    const float vMin = 2.5f;
    const float vMax = 3.65f;
    const float vStep = (vMax - vMin) / 127.0f;

    QVector<QPointF> points(128);
    for (int i = 0; i < 128; i++) {
        points[i] = QPointF(static_cast<double>(vMin + vStep * i),
                            static_cast<double>(ic[i]));
    }

    m_icSeries->replace(points);

    /* Auto-range Y axis */
    float yMin = ic[0], yMax = ic[0];
    for (int i = 1; i < 128; i++) {
        if (ic[i] < yMin) yMin = ic[i];
        if (ic[i] > yMax) yMax = ic[i];
    }
    /* Add 10% padding */
    float yPad = (yMax - yMin) * 0.10f;
    if (yPad < 0.001f) yPad = 1.0f;  /* flat curve fallback */
    m_axisY_ic->setRange(static_cast<double>(yMin - yPad),
                         static_cast<double>(yMax + yPad));
}

void ChartWidget::appendData(double timeSec, double voltage, double current)
{
    if (m_mode != VoltageCurrent)
        setMode(VoltageCurrent);

    if (m_voltageSeries)
        m_voltageSeries->append(timeSec, voltage);
    if (m_currentSeries)
        m_currentSeries->append(timeSec, current);

    m_lastTime = timeSec;

    /* Remove points older than the rolling window */
    double cutoff = timeSec - static_cast<double>(CHART_WINDOW_SECONDS);

    if (m_voltageSeries) {
        while (m_voltageSeries->count() > 0) {
            QPointF p = m_voltageSeries->at(0);
            if (p.x() < cutoff)
                m_voltageSeries->remove(0);
            else
                break;
        }
    }

    if (m_currentSeries) {
        while (m_currentSeries->count() > 0) {
            QPointF p = m_currentSeries->at(0);
            if (p.x() < cutoff)
                m_currentSeries->remove(0);
            else
                break;
        }
    }

    updateAxes(timeSec);
}

void ChartWidget::updateAxes(double timeSec)
{
    if (m_mode != VoltageCurrent || !m_axisX)
        return;

    double window = static_cast<double>(CHART_WINDOW_SECONDS);
    double left = std::max(0.0, timeSec - window);
    double right = std::max(window, timeSec + 5.0);

    if (right - left < window)
        right = left + window;

    m_axisX->setRange(left, right);
}

void ChartWidget::clear()
{
    if (m_voltageSeries) m_voltageSeries->clear();
    if (m_currentSeries) m_currentSeries->clear();
    if (m_icSeries)      m_icSeries->clear();
}

void ChartWidget::setTitle(const QString &title)
{
    m_chart->setTitle(title);
}

void ChartWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}
