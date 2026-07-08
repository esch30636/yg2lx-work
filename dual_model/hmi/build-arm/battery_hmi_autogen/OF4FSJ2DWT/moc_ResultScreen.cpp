/****************************************************************************
** Meta object code from reading C++ file 'ResultScreen.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "ui/screens/ResultScreen.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ResultScreen.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_ResultScreen_t {
    QByteArrayData data[34];
    char stringdata0[356];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ResultScreen_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ResultScreen_t qt_meta_stringdata_ResultScreen = {
    {
QT_MOC_LITERAL(0, 0, 12), // "ResultScreen"
QT_MOC_LITERAL(1, 13, 11), // "backClicked"
QT_MOC_LITERAL(2, 25, 0), // ""
QT_MOC_LITERAL(3, 26, 11), // "stopClicked"
QT_MOC_LITERAL(4, 38, 10), // "setIcCurve"
QT_MOC_LITERAL(5, 49, 16), // "const float[128]"
QT_MOC_LITERAL(6, 66, 2), // "ic"
QT_MOC_LITERAL(7, 69, 22), // "appendOscilloscopeData"
QT_MOC_LITERAL(8, 92, 7), // "timeSec"
QT_MOC_LITERAL(9, 100, 7), // "voltage"
QT_MOC_LITERAL(10, 108, 7), // "current"
QT_MOC_LITERAL(11, 116, 9), // "setHealth"
QT_MOC_LITERAL(12, 126, 5), // "value"
QT_MOC_LITERAL(13, 132, 10), // "confidence"
QT_MOC_LITERAL(14, 143, 14), // "setTemperature"
QT_MOC_LITERAL(15, 158, 5), // "tempC"
QT_MOC_LITERAL(16, 164, 11), // "setSwelling"
QT_MOC_LITERAL(17, 176, 7), // "swollen"
QT_MOC_LITERAL(18, 184, 9), // "setStatus"
QT_MOC_LITERAL(19, 194, 4), // "text"
QT_MOC_LITERAL(20, 199, 12), // "setConverged"
QT_MOC_LITERAL(21, 212, 9), // "converged"
QT_MOC_LITERAL(22, 222, 8), // "finalSoh"
QT_MOC_LITERAL(23, 231, 11), // "finalCiHalf"
QT_MOC_LITERAL(24, 243, 14), // "showCompletion"
QT_MOC_LITERAL(25, 258, 7), // "samples"
QT_MOC_LITERAL(26, 266, 6), // "stddev"
QT_MOC_LITERAL(27, 273, 7), // "natural"
QT_MOC_LITERAL(28, 281, 17), // "showCnnCompletion"
QT_MOC_LITERAL(29, 299, 8), // "finalRul"
QT_MOC_LITERAL(30, 308, 15), // "finalConfidence"
QT_MOC_LITERAL(31, 324, 5), // "stage"
QT_MOC_LITERAL(32, 330, 14), // "inferenceCount"
QT_MOC_LITERAL(33, 345, 10) // "clearChart"

    },
    "ResultScreen\0backClicked\0\0stopClicked\0"
    "setIcCurve\0const float[128]\0ic\0"
    "appendOscilloscopeData\0timeSec\0voltage\0"
    "current\0setHealth\0value\0confidence\0"
    "setTemperature\0tempC\0setSwelling\0"
    "swollen\0setStatus\0text\0setConverged\0"
    "converged\0finalSoh\0finalCiHalf\0"
    "showCompletion\0samples\0stddev\0natural\0"
    "showCnnCompletion\0finalRul\0finalConfidence\0"
    "stage\0inferenceCount\0clearChart"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ResultScreen[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   74,    2, 0x06 /* Public */,
       3,    0,   75,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       4,    1,   76,    2, 0x0a /* Public */,
       7,    3,   79,    2, 0x0a /* Public */,
      11,    2,   86,    2, 0x0a /* Public */,
      14,    1,   91,    2, 0x0a /* Public */,
      16,    1,   94,    2, 0x0a /* Public */,
      18,    1,   97,    2, 0x0a /* Public */,
      20,    3,  100,    2, 0x0a /* Public */,
      24,    5,  107,    2, 0x0a /* Public */,
      28,    5,  118,    2, 0x0a /* Public */,
      33,    0,  129,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 5,    6,
    QMetaType::Void, QMetaType::Double, QMetaType::Double, QMetaType::Double,    8,    9,   10,
    QMetaType::Void, QMetaType::Float, QMetaType::Float,   12,   13,
    QMetaType::Void, QMetaType::Float,   15,
    QMetaType::Void, QMetaType::Bool,   17,
    QMetaType::Void, QMetaType::QString,   19,
    QMetaType::Void, QMetaType::Bool, QMetaType::Float, QMetaType::Float,   21,   22,   23,
    QMetaType::Void, QMetaType::Float, QMetaType::Float, QMetaType::Int, QMetaType::Float, QMetaType::Bool,   22,   23,   25,   26,   27,
    QMetaType::Void, QMetaType::Float, QMetaType::Float, QMetaType::Int, QMetaType::Int, QMetaType::Bool,   29,   30,   31,   32,   27,
    QMetaType::Void,

       0        // eod
};

void ResultScreen::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ResultScreen *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->backClicked(); break;
        case 1: _t->stopClicked(); break;
        case 2: _t->setIcCurve((*reinterpret_cast< const float(*)[128]>(_a[1]))); break;
        case 3: _t->appendOscilloscopeData((*reinterpret_cast< double(*)>(_a[1])),(*reinterpret_cast< double(*)>(_a[2])),(*reinterpret_cast< double(*)>(_a[3]))); break;
        case 4: _t->setHealth((*reinterpret_cast< float(*)>(_a[1])),(*reinterpret_cast< float(*)>(_a[2]))); break;
        case 5: _t->setTemperature((*reinterpret_cast< float(*)>(_a[1]))); break;
        case 6: _t->setSwelling((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 7: _t->setStatus((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 8: _t->setConverged((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< float(*)>(_a[2])),(*reinterpret_cast< float(*)>(_a[3]))); break;
        case 9: _t->showCompletion((*reinterpret_cast< float(*)>(_a[1])),(*reinterpret_cast< float(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3])),(*reinterpret_cast< float(*)>(_a[4])),(*reinterpret_cast< bool(*)>(_a[5]))); break;
        case 10: _t->showCnnCompletion((*reinterpret_cast< float(*)>(_a[1])),(*reinterpret_cast< float(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3])),(*reinterpret_cast< int(*)>(_a[4])),(*reinterpret_cast< bool(*)>(_a[5]))); break;
        case 11: _t->clearChart(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ResultScreen::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ResultScreen::backClicked)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (ResultScreen::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ResultScreen::stopClicked)) {
                *result = 1;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject ResultScreen::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ResultScreen.data,
    qt_meta_data_ResultScreen,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ResultScreen::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ResultScreen::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ResultScreen.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ResultScreen::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void ResultScreen::backClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ResultScreen::stopClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
