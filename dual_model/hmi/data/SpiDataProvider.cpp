/*===========================================================================
 * SpiDataProvider.cpp — RA8 MCU data source over SPI bus (implementation)
 *
 * Dependencies:
 *   Linux spidev           (<linux/spi/spidev.h>)
 *   POSIX file I/O         (<fcntl.h>, <unistd.h>)
 *   sys/statvfs            (local NAND storage query)
 *
 * Cross-compilation note:
 *   When built with the Yocto SDK for aarch64, <linux/spi/spidev.h>
 *   comes from the kernel headers in the sysroot. On the host (x86_64),
 *   it comes from linux-libc-dev. The API is stable across kernel versions.
 *===========================================================================*/
#include "SpiDataProvider.h"
#include "config/AppConfig.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>

/* ── POSIX / Linux SPI ── */
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <linux/spi/spidev.h>

/* ═══════════════════════════════════════════════════════════════════
 * Construction / Destruction
 * ═══════════════════════════════════════════════════════════════════ */

SpiDataProvider::SpiDataProvider(const Config &config)
    : m_config(config)
    , m_fd(-1)
    , m_mockMode(false)
    , m_lastSeq(0)
    , m_sampleCount(0)
    , m_errorCount(0)
    , m_crcErrorCount(0)
    , m_consecutiveCrcErrors(0)
    , m_mockTick(0)
    , m_mockStorageBytes(10ULL * 1024 * 1024 * 1024)  /* 10 GB */
{
    if (!spiOpen()) {
        /* SPI device unavailable — transparent mock fallback */
        m_mockMode = true;
        fprintf(stderr, "[SpiDataProvider] %s: %s — falling back to mock mode\n",
                qPrintable(m_config.device), strerror(errno));
        printf("[SpiDataProvider] Using mock mode (synthetic RA8-like data)\n");
    } else {
        printf("[SpiDataProvider] Opened %s @ %u Hz, mode=%u\n",
               qPrintable(m_config.device), m_config.speedHz, m_config.mode);
    }
}

SpiDataProvider::~SpiDataProvider()
{
    spiClose();
    printf("[SpiDataProvider] Shutdown: %u samples, %u errors (%u CRC)\n",
           m_sampleCount, m_errorCount, m_crcErrorCount);
}

/* ═══════════════════════════════════════════════════════════════════
 * SPI Bus Management
 * ═══════════════════════════════════════════════════════════════════ */

bool SpiDataProvider::spiOpen()
{
    const char *path = qPrintable(m_config.device);
    uint8_t mode, bits, rmode, rbits;
    uint32_t speed, rspeed;

    m_fd = open(path, O_RDWR);
    if (m_fd < 0) return false;

    /* Configure SPI mode */
    mode = m_config.mode;
    if (ioctl(m_fd, SPI_IOC_WR_MODE, &mode) < 0) {
        fprintf(stderr, "[SpiDataProvider] SPI_IOC_WR_MODE failed: %s\n", strerror(errno));
        goto fail;
    }

    /* Configure bits per word */
    bits = m_config.bitsPerWord;
    if (ioctl(m_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        fprintf(stderr, "[SpiDataProvider] SPI_IOC_WR_BITS_PER_WORD failed: %s\n", strerror(errno));
        goto fail;
    }

    /* Configure clock speed */
    speed = m_config.speedHz;
    if (ioctl(m_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        fprintf(stderr, "[SpiDataProvider] SPI_IOC_WR_MAX_SPEED_HZ failed: %s\n", strerror(errno));
        goto fail;
    }

    /* Verify settings were applied (read-back) */
    ioctl(m_fd, SPI_IOC_RD_MODE, &rmode);
    ioctl(m_fd, SPI_IOC_RD_BITS_PER_WORD, &rbits);
    ioctl(m_fd, SPI_IOC_RD_MAX_SPEED_HZ, &rspeed);
    printf("[SpiDataProvider] SPI config verified: mode=%u bits=%u speed=%u Hz\n",
           rmode, rbits, rspeed);

    return true;

fail:
    close(m_fd);
    m_fd = -1;
    return false;
}

void SpiDataProvider::spiClose()
{
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * SPI Full-Duplex Transfer
 * ═══════════════════════════════════════════════════════════════════ */

bool SpiDataProvider::spiTransfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (m_fd < 0) return false;

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf        = reinterpret_cast<unsigned long>(tx);
    tr.rx_buf        = reinterpret_cast<unsigned long>(rx);
    tr.len           = static_cast<uint32_t>(len);
    tr.speed_hz      = m_config.speedHz;
    tr.delay_usecs   = 0;
    tr.bits_per_word = m_config.bitsPerWord;

    int ret = ioctl(m_fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 0) {
        fprintf(stderr, "[SpiDataProvider] SPI_IOC_MESSAGE failed: %s (len=%zu)\n",
                strerror(errno), len);
        return false;
    }

    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * CRC32 — IEEE 802.3 / MPEG-2 (polynomial 0x04C11DB7, reflected)
 *
 * This is the standard CRC-32 used by Ethernet, gzip, PNG, etc.
 * The lookup table is generated once (thread-safe — read-only after init).
 * ═══════════════════════════════════════════════════════════════════ */

uint32_t SpiDataProvider::crc32(const uint8_t *data, size_t len)
{
    return crc32Partial(0xFFFFFFFFu, data, len) ^ 0xFFFFFFFFu;
}

uint32_t SpiDataProvider::crc32Partial(uint32_t crc, const uint8_t *data, size_t len)
{
    /* Reflected polynomial 0xEDB88320 */
    static const uint32_t table[256] = {
        0x00000000u,0x77073096u,0xEE0E612Cu,0x990951BAu,0x076DC419u,0x706AF48Fu,0xE963A535u,0x9E6495A3u,
        0x0EDB8832u,0x79DCB8A4u,0xE0D5E91Eu,0x97D2D988u,0x09B64C2Bu,0x7EB17CBDu,0xE7B82D07u,0x90BF1D91u,
        0x1DB71064u,0x6AB020F2u,0xF3B97148u,0x84BE41DEu,0x1ADAD47Du,0x6DDDE4EBu,0xF4D4B551u,0x83D385C7u,
        0x136C9856u,0x646BA8C0u,0xFD62F97Au,0x8A65C9ECu,0x14015C4Fu,0x63066CD9u,0xFA0F3D63u,0x8D080DF5u,
        0x3B6E20C8u,0x4C69105Eu,0xD56041E4u,0xA2677172u,0x3C03E4D1u,0x4B04D447u,0xD20D85FDu,0xA50AB56Bu,
        0x35B5A8FAu,0x42B2986Cu,0xDBBBC9D6u,0xACBCF940u,0x32D86CE3u,0x45DF5C75u,0xDCD60DCFu,0xABD13D59u,
        0x26D930ACu,0x51DE003Au,0xC8D75180u,0xBFD06116u,0x21B4F4B5u,0x56B3C423u,0xCFBA9599u,0xB8BDA50Fu,
        0x2802B89Eu,0x5F058808u,0xC60CD9B2u,0xB10BE924u,0x2F6F7C87u,0x58684C11u,0xC1611DABu,0xB6662D3Du,
        0x76DC4190u,0x01DB7106u,0x98D220BCu,0xEFD5102Au,0x71B18589u,0x06B6B51Fu,0x9FBFE4A5u,0xE8B8D433u,
        0x7807C9A2u,0x0F00F934u,0x9609A88Eu,0xE10E9818u,0x7F6A0DBBu,0x086D3D2Du,0x91646C97u,0xE6635C01u,
        0x6B6B51F4u,0x1C6C6162u,0x856530D8u,0xF262004Eu,0x6C0695EDu,0x1B01A57Bu,0x8208F4C1u,0xF50FC457u,
        0x65B0D9C6u,0x12B7E950u,0x8BBEB8EAu,0xFCB9887Cu,0x62DD1DDFu,0x15DA2D49u,0x8CD37CF3u,0xFBD44C65u,
        0x4DB26158u,0x3AB551CEu,0xA3BC0074u,0xD4BB30E2u,0x4ADFA541u,0x3DD895D7u,0xA4D1C46Du,0xD3D6F4FBu,
        0x4369E96Au,0x346ED9FCu,0xAD678846u,0xDA60B8D0u,0x44042D73u,0x33031DE5u,0xAA0A4C5Fu,0xDD0D7CC9u,
        0x5005713Cu,0x270241AAu,0xBE0B1010u,0xC90C2086u,0x5768B525u,0x206F85B3u,0xB966D409u,0xCE61E49Fu,
        0x5EDEF90Eu,0x29D9C998u,0xB0D09822u,0xC7D7A8B4u,0x59B33D17u,0x2EB40D81u,0xB7BD5C3Bu,0xC0BA6CADu,
        0xEDB88320u,0x9ABFB3B6u,0x03B6E20Cu,0x74B1D29Au,0xEAD54739u,0x9DD277AFu,0x04DB2615u,0x73DC1683u,
        0xE3630B12u,0x94643B84u,0x0D6D6A3Eu,0x7A6A5AA8u,0xE40ECF0Bu,0x9309FF9Du,0x0A00AE27u,0x7D079EB1u,
        0xF00F9344u,0x8708A3D2u,0x1E01F268u,0x6906C2FEu,0xF762575Du,0x806567CBu,0x196C3671u,0x6E6B06E7u,
        0xFED41B76u,0x89D32BE0u,0x10DA7A5Au,0x67DD4ACCu,0xF9B9DF6Fu,0x8EBEEFF9u,0x17B7BE43u,0x60B08ED5u,
        0xD6D6A3E8u,0xA1D1937Eu,0x38D8C2C4u,0x4FDFF252u,0xD1BB67F1u,0xA6BC5767u,0x3FB506DDu,0x48B2364Bu,
        0xD80D2BDAu,0xAF0A1B4Cu,0x36034AF6u,0x41047A60u,0xDF60EFC3u,0xA867DF55u,0x316E8EEFu,0x4669BE79u,
        0xCB61B38Cu,0xBC66831Au,0x256FD2A0u,0x5268E236u,0xCC0C7795u,0xBB0B4703u,0x220216B9u,0x5505262Fu,
        0xC5BA3BBEu,0xB2BD0B28u,0x2BB45A92u,0x5CB30A04u,0xC2D7FFA7u,0xB5D0CF31u,0x2CD99E8Bu,0x5BDEAE1Du,
        0x9B64C2B0u,0xEC63F226u,0x756AA39Cu,0x026D930Au,0x9C0906A9u,0xEB0E363Fu,0x72076785u,0x05005713u,
        0x95BF4A82u,0xE2B87A14u,0x7BB12BAEu,0x0CB61B38u,0x92D28E9Bu,0xE5D5BE0Du,0x7CDCEFB7u,0x0BDBDF21u,
        0x86D3D2D4u,0xF1D4E242u,0x68DDB3F8u,0x1FDA836Eu,0x81BE16CDu,0xF6B9265Bu,0x6FB077E1u,0x18B74777u,
        0x88085AE6u,0xFF0F6A70u,0x66063BCAu,0x11010B5Cu,0x8F659EFFu,0xF862AE69u,0x610BFFD3u,0x160CDE45u,
        0xA3019B36u,0xD406A100u,0x4D0DE4BAu,0x3A0AD42Cu,0xA46D418Fu,0xD36A7109u,0x4A8D20B3u,0x3D8A1025u,
        0xAD350DB4u,0xDA323D22u,0x43076C98u,0x34005C0Eu,0xAA6CC9ADu,0xDD6BF93Bu,0x4462A881u,0x33659817u,
        0xBE6D95E2u,0xC96AA574u,0x5023F4CEu,0x2724C458u,0xB96051FBu,0xCE39606Du,0x573031D7u,0x20370141u,
        0xB0A61CD0u,0xC7A12C46u,0x5EA87DFCu,0x29AF4D6Au,0xB7E3D8C9u,0xC0E4C85Fu,0x599799E5u,0x2E90A973u,
    };

    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

/* ═══════════════════════════════════════════════════════════════════
 * Frame Validation
 * ═══════════════════════════════════════════════════════════════════ */

bool SpiDataProvider::validateFrame(const uint8_t *frame, size_t len)
{
    if (len < FRAME_SIZE) {
        fprintf(stderr, "[SpiDataProvider] Short frame: %zu < %zu\n", len, FRAME_SIZE);
        return false;
    }

    /* Check magic number */
    uint32_t magic;
    memcpy(&magic, frame, sizeof(magic));
    if (magic != FRAME_MAGIC) {
        /* Only log every 100th magic mismatch to avoid spam */
        if ((m_errorCount % 100) == 0) {
            fprintf(stderr, "[SpiDataProvider] Bad magic: 0x%08X (expected 0x%08X)\n",
                    magic, FRAME_MAGIC);
        }
        return false;
    }

    /* Check CRC32 (covers bytes 0..1075, CRC is at 1076..1079) */
    uint32_t expected_crc, actual_crc;
    memcpy(&expected_crc, frame + 1076, sizeof(expected_crc));
    actual_crc = crc32(frame, 1076);

    if (expected_crc != actual_crc) {
        m_crcErrorCount++;
        m_consecutiveCrcErrors++;
        if ((m_crcErrorCount % 50) == 1) {
            fprintf(stderr, "[SpiDataProvider] CRC mismatch: expected 0x%08X, "
                    "computed 0x%08X (total CRC errors: %u)\n",
                    expected_crc, actual_crc, m_crcErrorCount);
        }

        /* Auto-reset bus on consecutive CRC failures */
        if (m_config.maxCrcErrors > 0 && m_consecutiveCrcErrors >= m_config.maxCrcErrors) {
            fprintf(stderr, "[SpiDataProvider] %u consecutive CRC errors — "
                    "resetting SPI bus\n", m_consecutiveCrcErrors);
            spiClose();
            if (spiOpen()) {
                m_consecutiveCrcErrors = 0;
                printf("[SpiDataProvider] SPI bus reset successful\n");
            } else {
                fprintf(stderr, "[SpiDataProvider] SPI bus reset FAILED — "
                        "switching to mock mode\n");
                m_mockMode = true;
            }
        }
        return false;
    }

    m_consecutiveCrcErrors = 0;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * Frame Parsing → BatterySample
 * ═══════════════════════════════════════════════════════════════════ */

bool SpiDataProvider::parseFrame(const uint8_t *frame, size_t len,
                                  BatterySample &out)
{
    if (!validateFrame(frame, len)) return false;

    size_t off = 4;  /* skip magic */

    /* Sequence number */
    uint32_t seq;
    memcpy(&seq, frame + off, sizeof(seq));
    off += sizeof(seq);

    /* Detect duplicate or stale frame */
    if (seq != 0 && seq <= m_lastSeq && m_lastSeq != 0) {
        /* Duplicate — silently ignore */
        return false;
    }
    m_lastSeq = seq;

    /* Reserved (4 bytes) */
    uint8_t status = frame[off];  /* byte 0 of reserved is status */
    if (status == STATUS_NO_DATA) {
        return false;  /* Slave has no new data */
    }
    off += 4;

    /* IC curve */
    memcpy(out.ic_curve, frame + off, 128 * sizeof(float));
    off += 128 * sizeof(float);

    /* PINN features */
    memcpy(out.features, frame + off, 132 * sizeof(float));
    off += 132 * sizeof(float);

    /* Scalar fields */
    memcpy(&out.temperature,    frame + off,     sizeof(float));  off += 4;
    memcpy(&out.voltage,        frame + off,     sizeof(float));  off += 4;
    memcpy(&out.current,        frame + off,     sizeof(float));  off += 4;
    memcpy(&out.cycle_count,    frame + off,     sizeof(uint32_t)); off += 4;
    memcpy(&out.capacity_mah,   frame + off,     sizeof(float));  off += 4;
    memcpy(&out.cell_swelling,  frame + off,     sizeof(float));  off += 4;

    /* Timestamp: generated locally (RA8 has no RTC) */
    out.timestamp_ms = static_cast<int64_t>(m_sampleCount)
                       * static_cast<int64_t>(DATA_ACQUISITION_MS);

    /* Sanity check — clamp obviously broken values */
    if (out.temperature < -40.0f || out.temperature > 150.0f) {
        out.temperature = 25.0f;
    }
    if (out.voltage < 0.0f || out.voltage > 10.0f) {
        out.voltage = BATTERY_NOMINAL_V;
    }
    if (out.cell_swelling < 0.0f || out.cell_swelling > 1.0f) {
        out.cell_swelling = 0.0f;
    }
    if (out.capacity_mah < 0.0f) {
        out.capacity_mah = 0.0f;
    }

    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * DataProvider Interface
 * ═══════════════════════════════════════════════════════════════════ */

bool SpiDataProvider::read(BatterySample &sample)
{
    if (m_mockMode) {
        mockGenerate(sample);
        return true;
    }

    /* Prepare TX buffer: all zeros (dummy bytes to clock data IN) */
    uint8_t tx[FRAME_SIZE];
    uint8_t rx[FRAME_SIZE];
    memset(tx, 0, FRAME_SIZE);
    memset(rx, 0, FRAME_SIZE);

    if (!spiTransfer(tx, rx, FRAME_SIZE)) {
        m_errorCount++;
        return false;
    }

    if (!parseFrame(rx, FRAME_SIZE, sample)) {
        m_errorCount++;
        return false;
    }

    m_sampleCount++;
    return true;
}

bool SpiDataProvider::readStorage(StorageInfo &info)
{
    return queryLocalStorage(info);
}

QString SpiDataProvider::name() const
{
    if (m_mockMode) {
        return QString("SPI/RA8 (mock)");
    }
    return QString("SPI/RA8 (%1)").arg(m_config.device);
}

void SpiDataProvider::reset()
{
    m_lastSeq = 0;
    m_errorCount = 0;
    m_crcErrorCount = 0;
    m_consecutiveCrcErrors = 0;
    m_mockTick = 0;
    m_mockStorageBytes = 10ULL * 1024 * 1024 * 1024;
}

/* ═══════════════════════════════════════════════════════════════════
 * Mock Data Generation (RA8-like synthetic telemetry)
 *
 * Produces IC curves and features that resemble the DemoDataProvider
 * but with different noise characteristics — mimicking what the RA8
 * ADC would produce (slightly higher noise floor, quantization effects).
 * ═══════════════════════════════════════════════════════════════════ */

void SpiDataProvider::mockGenerate(BatterySample &sample)
{
    memset(&sample, 0, sizeof(sample));

    /* ── Simulated SOH: linear decay ── */
    float soh = 1.0f - static_cast<float>(m_mockTick) * 0.00003f;
    if (soh < 0.15f) soh = 0.15f;

    /* ── IC curve: Gaussian peak with ADC-like noise ── */
    {
        /* Deterministic pseudo-random from tick */
        uint64_t seed = m_mockTick;
        auto randf = [&seed]() -> float {
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            return static_cast<float>((seed >> 32) & 0xFFFFFFFFu) / 4294967296.0f;
        };

        float peakH = 2.8f * soh;
        float peakP = 64.0f + (1.0f - soh) * 8.0f;
        float peakW = 18.0f + (1.0f - soh) * 2.0f;

        for (int i = 0; i < 128; i++) {
            float x = (static_cast<float>(i) - peakP) / peakW;
            float ic = peakH * expf(-x * x * 0.5f)
                     + 0.1f * sinf(x * 4.0f)
                     + (randf() - 0.5f) * 0.04f;  /* slight ADC noise */
            sample.ic_curve[i] = ic;
        }

        /* ── Features ── */
        memcpy(sample.features, sample.ic_curve, 128 * sizeof(float));
        sample.features[128] = 28.0f + (randf() - 0.5f) * 5.0f;    /* temp ~25-31°C */
        sample.features[129] = log10f(static_cast<float>(m_mockTick / 10) + 1.0f); /* log cycle */
        sample.features[130] = -0.03f - (1.0f - soh) * 0.08f + (randf() - 0.5f) * 0.002f;
        sample.features[131] = (BATTERY_NOMINAL_MAH * (0.85f + 0.15f * soh)) / BATTERY_NOMINAL_MAH;

        /* ── Scalar telemetry ── */
        sample.temperature   = sample.features[128];
        sample.voltage       = BATTERY_CUTOFF_V + (BATTERY_CHARGE_V - BATTERY_CUTOFF_V)
                             * static_cast<float>((m_mockTick % 300)) / 300.0f
                             + (randf() - 0.5f) * 0.04f;
        sample.current       = ((m_mockTick % 600) < 300) ? 50.0f : -50.0f;
        sample.cycle_count   = static_cast<uint32_t>(m_mockTick / 600);
        sample.capacity_mah  = BATTERY_NOMINAL_MAH * (0.85f + 0.15f * soh);
        sample.cell_swelling = (randf() < 0.02f) ? randf() * 0.5f : 0.0f;
        sample.timestamp_ms  = static_cast<int64_t>(m_mockTick)
                             * static_cast<int64_t>(DATA_ACQUISITION_MS);
    }

    /* ── Storage growth ── */
    m_mockStorageBytes += 1024;
    uint64_t maxBytes = static_cast<uint64_t>(NAND_TOTAL_GB * 1024.0 * 1024.0 * 1024.0);
    if (m_mockStorageBytes > maxBytes) m_mockStorageBytes = maxBytes;

    m_mockTick++;
}

/* ═══════════════════════════════════════════════════════════════════
 * Local Storage Query (statvfs on NAND mount point)
 * ═══════════════════════════════════════════════════════════════════ */

bool SpiDataProvider::queryLocalStorage(StorageInfo &info)
{
    /* Try common NAND mount points in order */
    static const char *mountPoints[] = {
        "/data",
        "/mnt/nand",
        "/home",
        "/",
        nullptr
    };

    struct statvfs st;
    bool ok = false;

    for (const char **mp = mountPoints; *mp != nullptr; mp++) {
        if (statvfs(*mp, &st) == 0) {
            ok = true;
            break;
        }
    }

    if (!ok) {
        /* All mount points failed — return mock data */
        uint64_t total = static_cast<uint64_t>(NAND_TOTAL_GB * 1024.0f * 1024.0f * 1024.0f);
        info.total_bytes   = total;
        info.used_bytes    = m_mockStorageBytes;
        info.free_bytes    = total - m_mockStorageBytes;
        info.usage_percent = 100.0f * static_cast<float>(m_mockStorageBytes)
                           / static_cast<float>(total);
        info.bad_blocks    = static_cast<uint32_t>(m_mockTick / 5000);
        info.total_blocks  = 4096;
        return true;
    }

    uint64_t total = st.f_blocks * st.f_frsize;
    uint64_t avail = st.f_bavail * st.f_frsize;
    uint64_t used  = total - avail;

    info.total_bytes   = total;
    info.used_bytes    = used;
    info.free_bytes    = avail;
    info.usage_percent = (total > 0) ? 100.0f * static_cast<float>(used) / static_cast<float>(total) : 0.0f;
    info.bad_blocks    = 0;       /* statvfs doesn't report bad blocks */
    info.total_blocks  = static_cast<uint32_t>(st.f_blocks);

    return true;
}
