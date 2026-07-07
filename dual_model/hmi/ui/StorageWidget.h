/*===========================================================================
 * StorageWidget.h — Module 5: Octa-NAND storage status monitoring
 *
 * Shows NAND flash usage with GB-level detail and bad block count.
 *===========================================================================*/
#ifndef HMI_UI_STORAGE_WIDGET_H
#define HMI_UI_STORAGE_WIDGET_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>

class StorageWidget : public QWidget {
    Q_OBJECT
public:
    explicit StorageWidget(QWidget *parent = nullptr);

    /** Update from StorageInfo struct */
    void setUsage(float percent, uint64_t usedBytes, uint64_t totalBytes,
                  uint32_t badBlocks, uint32_t totalBlocks);

private:
    void setupUi();
    static QString formatBytes(uint64_t bytes);

    QLabel       *m_titleLabel;
    QProgressBar *m_progressBar;
    QLabel       *m_percentLabel;
    QLabel       *m_detailLabel;
    QLabel       *m_badBlockLabel;
    QString       m_lastStorageBarStyle;  /* avoids redundant GPU re-polish */
};

#endif /* HMI_UI_STORAGE_WIDGET_H */
