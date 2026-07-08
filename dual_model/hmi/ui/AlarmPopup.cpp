/*===========================================================================
 * AlarmPopup.cpp — Alarm popup dialog implementation
 *===========================================================================*/
#include "AlarmPopup.h"
#include "config/AppConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QApplication>
#include <QScreen>

static int s_instanceCounter = 0;

AlarmPopup::AlarmPopup(AlarmId id, const QString &title,
                       const QString &description,
                       float currentValue, float threshold,
                       const QString &unit,
                       QWidget *parent)
    : QDialog(parent)
    , m_alarmId(id)
    , m_instanceId(s_instanceCounter++)
    , m_autoDismissTimer(nullptr)
    , m_iconLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_descLabel(nullptr)
    , m_valueLabel(nullptr)
    , m_ackBtn(nullptr)
{
    setObjectName("alarmPopup");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(false);

    setupUi(title, description, currentValue, threshold, unit);

    /* Auto-dismiss after 10 seconds */
    m_autoDismissTimer = new QTimer(this);
    m_autoDismissTimer->setSingleShot(true);
    m_autoDismissTimer->setInterval(10000);
    connect(m_autoDismissTimer, &QTimer::timeout, this, &AlarmPopup::onAutoDismiss);
}

AlarmPopup::~AlarmPopup() = default;

void AlarmPopup::setupUi(const QString &title, const QString &description,
                          float currentValue, float threshold, const QString &unit)
{
    /* Semi-transparent backdrop overlay */
    setStyleSheet(QString(
        "AlarmPopup { background: transparent; }"
    ));

    /* Card container */
    QFrame *card = new QFrame(this);
    card->setObjectName("alarmPopupCard");
    card->setStyleSheet(QString(
        "QFrame#alarmPopupCard {"
        "  background-color: %1;"
        "  border: 2px solid %2;"
        "  border-radius: 8px;"
        "}"
    ).arg(COLOR_BG_PANEL).arg(COLOR_CRITICAL_RED));
    card->setFixedSize(ALARM_POPUP_WIDTH, ALARM_POPUP_HEIGHT);

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(6);

    /* Title row */
    QHBoxLayout *titleRow = new QHBoxLayout();
    m_iconLabel = new QLabel("⚠");
    m_iconLabel->setStyleSheet(QString("font-size: 24px; color: %1;").arg(COLOR_CRITICAL_RED));
    m_titleLabel = new QLabel(title);
    m_titleLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(COLOR_CRITICAL_RED));
    titleRow->addWidget(m_iconLabel);
    titleRow->addWidget(m_titleLabel, 1);
    cardLayout->addLayout(titleRow);

    /* Description */
    m_descLabel = new QLabel(description);
    m_descLabel->setStyleSheet(QString("font-size: 13px; color: %1;").arg(COLOR_TEXT));
    m_descLabel->setWordWrap(true);
    cardLayout->addWidget(m_descLabel);

    /* Value display */
    m_valueLabel = new QLabel(tr("当前值: %1 %2  |  阈值: %3 %2")
        .arg(currentValue, 0, 'f', 2)
        .arg(unit)
        .arg(threshold, 0, 'f', 2));
    m_valueLabel->setStyleSheet(QString("font-size: 12px; color: %1; font-weight: bold;").arg(COLOR_WARNING_YELLOW));
    cardLayout->addWidget(m_valueLabel);

    cardLayout->addStretch();

    /* Acknowledge button */
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_ackBtn = new QPushButton(tr("确认"));
    m_ackBtn->setObjectName("acknowledge");
    m_ackBtn->setFixedHeight(34);
    m_ackBtn->setMinimumWidth(140);
    connect(m_ackBtn, &QPushButton::clicked, this, &AlarmPopup::onAcknowledge);
    btnRow->addWidget(m_ackBtn);
    cardLayout->addLayout(btnRow);

    /* Center the card in the popup */
    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(card, 0, Qt::AlignCenter);
}

void AlarmPopup::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    /* Position at center of parent or screen */
    if (parentWidget()) {
        QRect parentRect = parentWidget()->geometry();
        int x = parentRect.x() + (parentRect.width()  - ALARM_POPUP_WIDTH)  / 2;
        int y = parentRect.y() + (parentRect.height() - ALARM_POPUP_HEIGHT) / 2;
        move(x, y);
    } else {
        QScreen *screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenRect = screen->availableGeometry();
            move(screenRect.center() - QPoint(ALARM_POPUP_WIDTH / 2, ALARM_POPUP_HEIGHT / 2));
        }
    }

    /* Fade-in animation */
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity(0.0);
    setGraphicsEffect(effect);
    QPropertyAnimation *anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(200);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    /* Start auto-dismiss */
    m_autoDismissTimer->start();
}

void AlarmPopup::paintEvent(QPaintEvent *event)
{
    /* Semi-transparent backdrop */
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 80));
    QDialog::paintEvent(event);
}

void AlarmPopup::onAcknowledge()
{
    m_autoDismissTimer->stop();
    emit acknowledged(m_instanceId);
    close();
}

void AlarmPopup::onAutoDismiss()
{
    emit acknowledged(m_instanceId);
    close();
}
