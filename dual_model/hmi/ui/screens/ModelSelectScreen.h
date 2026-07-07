/*===========================================================================
 * ModelSelectScreen.h — Screen 2: Model selection (PINN vs CNN)
 *
 * Two large buttons for selecting the inference model.
 * Emits modelSelected(0=PINN, 1=CNN) on click.
 *===========================================================================*/
#ifndef HMI_UI_SCREENS_MODEL_SELECT_SCREEN_H
#define HMI_UI_SCREENS_MODEL_SELECT_SCREEN_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>

class ModelSelectScreen : public QWidget {
    Q_OBJECT
public:
    enum Model { PINN = 0, CNN = 1 };

    explicit ModelSelectScreen(QWidget *parent = nullptr);

signals:
    /** Emitted when user selects a model: 0=PINN, 1=CNN */
    void modelSelected(int model);

private:
    void setupUi();

    QLabel      *m_titleLabel;
    QPushButton *m_pinnButton;
    QPushButton *m_cnnButton;
};

#endif /* HMI_UI_SCREENS_MODEL_SELECT_SCREEN_H */
