/*===========================================================================
 * FileDataProvider.h — Binary file replay provider
 *
 * Reads .bin files in the same format as the CLI tool:
 *   - IC curve: 128 floats (512 bytes)
 *   - Features: 132 floats (528 bytes)
 *
 * Frames are concatenated in a single file. If only one frame exists,
 * it replays the same data repeatedly (steady-state simulation).
 *===========================================================================*/
#ifndef HMI_FILE_DATA_PROVIDER_H
#define HMI_FILE_DATA_PROVIDER_H

#include "DataProvider.h"
#include <QString>
#include <QFile>
#include <vector>

class FileDataProvider : public DataProvider {
public:
    /**
     * @param filePath  Path to .bin file with concatenated frames
     *                  (128+132 floats per frame). Single-frame files
     *                  are replayed repeatedly.
     */
    explicit FileDataProvider(const QString &filePath);
    ~FileDataProvider() override;

    bool read(BatterySample &sample) override;
    bool readStorage(StorageInfo &info) override;
    QString name() const override { return QString("File: %1").arg(m_filePath); }
    void reset() override;

    /** Returns false if the file could not be opened or parsed */
    bool isValid() const { return !m_frames.empty(); }

private:
    struct Frame {
        float ic_curve[128];
        float features[132];
    };

    QString m_filePath;
    std::vector<Frame> m_frames;
    size_t m_frameIndex;
    uint64_t m_tick;
    uint64_t m_storageBytesUsed;
};

#endif /* HMI_FILE_DATA_PROVIDER_H */
