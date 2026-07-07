/*===========================================================================
 * InitScreen.h — Screen 1: Device initialization with progress bar
 *
 * Displays a 3-second fake initialization progress bar, then enables
 * the "Start Test" button. Emits startTestClicked() when the user
 * clicks the button.
 *===========================================================================*/
#ifndef HMI_UI_SCREENS_INIT_SCREEN_H
#define HMI_UI_SCREENS_INIT_SCREEN_H

#include <QWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

class InitScreen : public QWidget {
    Q_OBJECT
public:
    explicit InitScreen(QWidget *parent = nullptr);

signals:
    /** Emitted when user clicks "Start Test" after init completes */
    void startTestClicked();

private slots:
    void onProgressTick();

private:
    void setupUi();

    QLabel       *m_titleLabel;
    QProgressBar *m_progressBar;
    QPushButton  *m_startButton;
    QTimer       *m_progressTimer;
    int           m_progressValue;
};

#endif /* HMI_UI_SCREENS_INIT_SCREEN_H */
