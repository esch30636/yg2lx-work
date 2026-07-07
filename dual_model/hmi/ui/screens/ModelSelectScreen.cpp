/*===========================================================================
 * ModelSelectScreen.cpp — Model selection screen implementation
 *===========================================================================*/
#include "ModelSelectScreen.h"
#include "config/AppConfig.h"
#include <QVBoxLayout>

ModelSelectScreen::ModelSelectScreen(QWidget *parent)
    : QWidget(parent)
    , m_titleLabel(nullptr)
    , m_pinnButton(nullptr)
    , m_cnnButton(nullptr)
{
    setupUi();
}

void ModelSelectScreen::setupUi()
{
    setStyleSheet(QString("background-color: %1;").arg(COLOR_BG_MAIN));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(60, 60, 60, 60);
    layout->setSpacing(20);

    /* ── Title ── */
    m_titleLabel = new QLabel(tr("Select Inference Model"));
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(QString(
        "color: %1; font-size: 22px; font-weight: bold;"
    ).arg(COLOR_TEXT));
    layout->addWidget(m_titleLabel);

    layout->addSpacing(30);

    /* ── PINN button ── */
    m_pinnButton = new QPushButton();
    m_pinnButton->setObjectName("primary");
    m_pinnButton->setMinimumHeight(80);
    m_pinnButton->setStyleSheet(QString(
        "QPushButton#primary {"
        "  font-size: 20px; font-weight: bold;"
        "  padding: 16px 32px;"
        "}"
        "\n"
        "QPushButton#primary QLabel {"
        "  color: %1; background: transparent;"
        "}"
    ).arg(COLOR_TEXT));

    /* Use rich text layout inside button */
    m_pinnButton->setText(tr("PINN  Fast Assessment\nSOH  ·  Seconds to Converge"));

    layout->addWidget(m_pinnButton);

    layout->addSpacing(16);

    /* ── CNN button ── */
    m_cnnButton = new QPushButton();
    m_cnnButton->setObjectName("primary");
    m_cnnButton->setMinimumHeight(80);
    m_cnnButton->setStyleSheet(QString(
        "QPushButton#primary {"
        "  font-size: 20px; font-weight: bold;"
        "  padding: 16px 32px;"
        "}"
        "\n"
        "QPushButton#primary QLabel {"
        "  color: %1; background: transparent;"
        "}"
    ).arg(COLOR_TEXT));

    m_cnnButton->setText(tr("CNN  Deep Analysis\nRUL  ·  Full Cycle Assessment"));

    layout->addWidget(m_cnnButton);

    layout->addStretch();

    /* ── Connections ── */
    connect(m_pinnButton, &QPushButton::clicked, this, [this]() {
        emit modelSelected(PINN);
    });
    connect(m_cnnButton, &QPushButton::clicked, this, [this]() {
        emit modelSelected(CNN);
    });
}
