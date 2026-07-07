/*===========================================================================
 * CycleProgressBar.h — Module 3: Cycle test progress display
 *
 * Shows current cycle / total cycles with elapsed and estimated remaining time.
 *===========================================================================*/
#ifndef HMI_UI_CYCLE_PROGRESS_BAR_H
#define HMI_UI_CYCLE_PROGRESS_BAR_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>

class CycleProgressBar : public QWidget {
    Q_OBJECT
public:
    explicit CycleProgressBar(QWidget *parent = nullptr);

    /** Update progress: current cycle count and total target */
    void setProgress(uint32_t current, uint32_t total);

    /** Update elapsed time (seconds since first sample) */
    void setElapsed(double elapsedSec);

    /** Set test status text */
    void setStatus(const QString &status);

private:
    void setupUi();

    QLabel       *m_titleLabel;
    QProgressBar *m_progressBar;
    QLabel       *m_countLabel;
    QLabel       *m_elapsedLabel;
    QLabel       *m_remainingLabel;
    QLabel       *m_statusLabel;

    uint32_t m_current;
    uint32_t m_total;
    double   m_elapsedSec;
};

#endif /* HMI_UI_CYCLE_PROGRESS_BAR_H */
