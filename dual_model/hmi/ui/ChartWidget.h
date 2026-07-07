/*===========================================================================
 * ChartWidget.h — Real-time chart widget (dual-mode)
 *
 * Mode 1 — Voltage/Current vs Time (legacy):
 *   Dual-Y-axis rolling time-series for charge/discharge monitoring.
 *
 * Mode 2 — IC Curve (dQ/dV vs Voltage):
 *   Fixed 128-point IC curve display. Full-replace on each update.
 *   X-axis: voltage range [2.5, 3.65]V (128 equidistant points).
 *   Y-axis: dQ/dV, auto-ranging.
 *
 * Uses QtCharts. Antialiasing disabled for embedded Mali-G31.
 *===========================================================================*/
#ifndef HMI_UI_CHART_WIDGET_H
#define HMI_UI_CHART_WIDGET_H

#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <deque>

class ChartWidget : public QWidget {
    Q_OBJECT
public:
    enum Mode {
        VoltageCurrent,   /* dual-Y V/A vs time (legacy) */
        IcCurve           /* single-series dQ/dV vs voltage */
    };

    explicit ChartWidget(QWidget *parent = nullptr);
    ~ChartWidget() override;

    /** Mode 1: Append V/A data point (shared timestamp) */
    void appendData(double timeSec, double voltage, double current);

    /** Mode 2: Replace entire IC curve (128-point dQ/dV array) */
    void setIcCurve(const float ic[128]);

    /** Switch display mode */
    void setMode(Mode mode);

    /** Clear all series data */
    void clear();

    /** Set chart title */
    void setTitle(const QString &title);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupChart();
    void setupVoltageCurrentMode();
    void setupIcCurveMode();
    void updateAxes(double timeSec);

    /* Qt Charts components */
    QtCharts::QChart       *m_chart;
    QtCharts::QChartView   *m_chartView;

    /* Mode 1: V/A vs Time */
    QtCharts::QLineSeries  *m_voltageSeries;
    QtCharts::QLineSeries  *m_currentSeries;
    QtCharts::QValueAxis   *m_axisY_voltage;
    QtCharts::QValueAxis   *m_axisY_current;
    QtCharts::QValueAxis   *m_axisX;           /* time (seconds) */

    /* Mode 2: IC Curve */
    QtCharts::QLineSeries  *m_icSeries;
    QtCharts::QValueAxis   *m_axisY_ic;        /* dQ/dV */
    QtCharts::QValueAxis   *m_axisX_ic;        /* voltage (V) */

    Mode  m_mode;
    double m_timeOffset;
    double m_lastTime;

    static constexpr int MAX_POINTS = 1500;     /* 5 min at 5 Hz */
};

#endif /* HMI_UI_CHART_WIDGET_H */
