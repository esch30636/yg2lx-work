/*===========================================================================
 * SpiDataProvider.h — RA8 MCU data source over SPI bus
 *
 * Reads real-time battery telemetry from an RA8 microcontroller connected
 * via SPI to the MYD-YG2LX host processor (RZ/G2L, Cortex-A55).
 *
 * Protocol (v1 placeholder — awaiting RA8 firmware finalization):
 *   Master (RZ/G2L) sends 1080 bytes of 0x00 (SPI full-duplex clock).
 *   Slave  (RA8) responds with a fixed-size SpiFrame (little-endian):
 *
 *     Offset   Size   Field
 *     ──────   ────   ─────
 *     0         4     magic       0x52413848 ("RA8H")
 *     4         4     sequence    monotonic frame counter
 *     8         4     reserved    (padding to 8-byte align payload)
 *     12      512     ic_curve[128]   float32
 *     524     528     features[132]   float32
 *     1052      4     temperature     float32  (°C)
 *     1056      4     voltage         float32  (V)
 *     1060      4     current         float32  (A)
 *     1064      4     cycle_count     uint32
 *     1068      4     capacity_mah    float32
 *     1072      4     cell_swelling   float32  (0.0–1.0)
 *     1076      4     crc32           IEEE 802.3 (over bytes 0..1075)
 *     ──────
 *     Total: 1080 bytes
 *
 *   Status byte (reserved[0]): 0x00 = valid data, 0xFF = no new data.
 *
 * Fallback: when the SPI device cannot be opened (no RA8 connected, or
 * running on host without SPI), the provider transparently falls back to
 * mock mode — generating synthetic data that mimics the RA8 data stream.
 * The name() reflects this: "SPI/RA8 (/dev/spidev0.0)" vs "SPI/RA8 (mock)".
 *
 * Storage queries (readStorage) use local statvfs on the board's NAND,
 * not the SPI bus — storage is local to the host, not on the RA8.
 *===========================================================================*/
#ifndef HMI_DATA_SPI_PROVIDER_H
#define HMI_DATA_SPI_PROVIDER_H

#include "DataProvider.h"
#include <QString>
#include <cstdint>

class SpiDataProvider : public DataProvider {
public:
    /* ── SPI bus configuration ── */
    struct Config {
        /** SPI device node, e.g. "/dev/spidev0.0" or "/dev/spidev1.0"
         *  RZ/G2L has up to 3 SPI channels (RSPI0–RSPI2). */
        QString device   = "/dev/spidev0.0";

        /** Bus clock in Hz. RA8 supports up to 20 MHz SPI; start
         *  conservatively at 5 MHz and tune based on signal integrity. */
        uint32_t speedHz = 5000000;

        /** SPI mode (CPOL | CPHA<<1):
         *  0 = idle low,  sample on leading  edge
         *  1 = idle low,  sample on trailing edge
         *  2 = idle high, sample on leading  edge
         *  3 = idle high, sample on trailing edge */
        uint8_t  mode    = 0;

        /** Bits per word. Almost always 8 for MCU-to-MPU SPI. */
        uint8_t  bitsPerWord = 8;

        /** Maximum time to wait for a full frame (ms). If a transfer
         *  takes longer than this, it is aborted and counted as an error. */
        uint32_t timeoutMs   = 100;

        /** Number of consecutive CRC failures before forcing a bus reset
         *  (close + reopen). 0 = never reset. */
        uint32_t maxCrcErrors = 10;
    };

    explicit SpiDataProvider(const Config &config);
    ~SpiDataProvider() override;

    /* ── DataProvider interface ── */
    bool    read(BatterySample &sample) override;
    bool    readStorage(StorageInfo &info) override;
    QString name() const override;
    void    reset() override;

    /* ── Diagnostics ── */
    bool     isOpen()       const { return m_fd >= 0; }
    bool     isMockMode()   const { return m_mockMode; }
    uint32_t sampleCount()  const { return m_sampleCount; }
    uint32_t errorCount()   const { return m_errorCount; }
    uint32_t crcErrorCount() const { return m_crcErrorCount; }

private:
    /* ── SPI primitives ── */
    bool spiOpen();
    void spiClose();
    bool spiTransfer(const uint8_t *tx, uint8_t *rx, size_t len);

    /* ── Protocol helpers ── */
    bool    validateFrame(const uint8_t *frame, size_t len);
    bool    parseFrame(const uint8_t *frame, size_t len, BatterySample &out);
    static uint32_t crc32(const uint8_t *data, size_t len);
    static uint32_t crc32Partial(uint32_t crc, const uint8_t *data, size_t len);

    /* ── Mock fallback ── */
    void mockGenerate(BatterySample &sample);

    /* ── Storage helper ── */
    bool queryLocalStorage(StorageInfo &info);

    Config   m_config;
    int      m_fd;
    bool     m_mockMode;
    uint32_t m_lastSeq;
    uint32_t m_sampleCount;
    uint32_t m_errorCount;
    uint32_t m_crcErrorCount;
    uint32_t m_consecutiveCrcErrors;
    uint64_t m_mockTick;
    uint64_t m_mockStorageBytes;

    static constexpr size_t    FRAME_SIZE   = 1080;
    static constexpr uint32_t  FRAME_MAGIC  = 0x52413848;  /* "RA8H" */
    static constexpr uint8_t   STATUS_NO_DATA = 0xFF;
};

#endif /* HMI_DATA_SPI_PROVIDER_H */
