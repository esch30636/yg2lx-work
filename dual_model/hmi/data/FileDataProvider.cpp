/*===========================================================================
 * FileDataProvider.cpp — Binary file replay implementation
 *===========================================================================*/
#include "FileDataProvider.h"
#include "config/AppConfig.h"
#include <cstring>

FileDataProvider::FileDataProvider(const QString &filePath)
    : m_filePath(filePath)
    , m_frameIndex(0)
    , m_tick(0)
    , m_storageBytesUsed(10ULL * 1024 * 1024 * 1024)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;  /* isValid() will return false */
    }

    const size_t frameSize = 128 * sizeof(float) + 132 * sizeof(float);
    const qint64 fileSize = file.size();

    if (fileSize < static_cast<qint64>(frameSize)) {
        return;  /* file too small */
    }

    size_t numFrames = static_cast<size_t>(fileSize) / frameSize;
    m_frames.resize(numFrames);

    QByteArray data = file.readAll();
    const float *raw = reinterpret_cast<const float*>(data.constData());

    for (size_t f = 0; f < numFrames; f++) {
        std::memcpy(m_frames[f].ic_curve, raw + f * 260,      128 * sizeof(float));
        std::memcpy(m_frames[f].features,  raw + f * 260 + 128, 132 * sizeof(float));
    }
}

FileDataProvider::~FileDataProvider() = default;

bool FileDataProvider::read(BatterySample &sample)
{
    if (m_frames.empty()) return false;

    /* If single frame, replay it repeatedly with synthetic telemetry */
    const Frame &frame = m_frames[m_frameIndex];
    std::memcpy(sample.ic_curve, frame.ic_curve, 128 * sizeof(float));
    std::memcpy(sample.features, frame.features, 132 * sizeof(float));

    /* Synthesize scalar telemetry (not in file) */
    sample.temperature    = 30.0f + (m_tick % 100) * 0.1f;
    sample.voltage        = 3.2f;
    sample.current        = 40.0f;
    sample.cycle_count    = static_cast<uint32_t>(m_frameIndex);
    sample.capacity_mah   = BATTERY_NOMINAL_MAH;
    sample.cell_swelling  = 0.0f;
    sample.timestamp_ms   = static_cast<int64_t>(m_tick) * DATA_ACQUISITION_MS;

    /* Advance frame index (loop for single-frame files) */
    m_frameIndex = (m_frameIndex + 1) % m_frames.size();

    m_storageBytesUsed += 1024;
    m_tick++;
    return true;
}

bool FileDataProvider::readStorage(StorageInfo &info)
{
    uint64_t total = static_cast<uint64_t>(NAND_TOTAL_GB * 1024.0f * 1024.0f * 1024.0f);
    info.total_bytes = total;
    info.used_bytes = m_storageBytesUsed;
    info.free_bytes = total - m_storageBytesUsed;
    info.usage_percent = 100.0f * static_cast<float>(m_storageBytesUsed) / static_cast<float>(total);
    info.bad_blocks = 0;
    info.total_blocks = 4096;
    return true;
}

void FileDataProvider::reset()
{
    m_frameIndex = 0;
    m_tick = 0;
}
