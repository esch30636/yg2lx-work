/*===========================================================================
 * UartDataProvider.h — RA8 real-time data via UART (CSV text)
 *
 * Reads battery telemetry from RA8 microcontroller over UART serial.
 * The RA8 sends one CSV line every 100ms:
 *   voltage,current,temperature,pressure\n
 *
 * This provider:
 *   1. Opens and configures the serial port (115200 8N1 raw)
 *   2. Parses each CSV line into scalar telemetry fields
 *   3. Accumulates V/I history for incremental capacity (IC) curve computation
 *   4. Detects charge/discharge cycles
 *   5. Generates 132-dim PINN feature vector + 128-point IC curve
 *
 * Usage:
 *   battery_hmi --uart /dev/ttySC3
 *===========================================================================*/
#ifndef HMI_DATA_UART_PROVIDER_H
#define HMI_DATA_UART_PROVIDER_H

#include "DataProvider.h"
#include <QString>
#include <cstdint>

class UartDataProvider : public DataProvider {
public:
    struct Config {
        /** Serial device node, e.g. "/dev/ttySC3" */
        QString device     = "/dev/ttySC3";

        /** Baud rate. Must match RA8 setting (115200). */
        uint32_t baudRate  = 115200;

        /** Maximum number of raw (V,I) samples kept in history for
         *  IC curve computation. 6000 samples = 10 minutes @ 10Hz. */
        uint32_t historySize = 6000;
    };

    explicit UartDataProvider(const Config &config);
    ~UartDataProvider() override;

    /* ── DataProvider interface ── */
    bool    read(BatterySample &sample) override;
    bool    readStorage(StorageInfo &info) override;
    QString name() const override;
    void    reset() override;

    /* ── Diagnostics ── */
    bool     isOpen()       const { return m_fd >= 0; }
    uint32_t sampleCount()  const { return m_sampleCount; }
    uint32_t parseErrorCount() const { return m_parseErrors; }

private:
    /* ── Serial port ── */
    bool uartOpen();
    void uartClose();
    int  uartReadLine(char *buf, size_t maxLen);

    /* ── CSV parsing ── */
    bool parseCsvLine(const char *line,
                      float &voltage, float &current,
                      float &temperature, float &pressure);

    /* ── Feature engineering ── */
    void   pushHistory(float voltage, float current, float dt);
    void   computeIcCurve(float *ic_out);
    void   detectCycleBoundary();
    void   fillFeatures(BatterySample &sample);

    Config   m_config;
    int      m_fd;
    uint32_t m_sampleCount;
    uint32_t m_parseErrors;
    uint64_t m_tick;

    /* ── Raw sample history (ring buffer) ── */
    struct RawSample {
        float voltage;
        float current;   /* signed: +charge, -discharge */
    };
    RawSample *m_history;
    uint32_t   m_historyCap;
    uint32_t   m_historyLen;
    uint32_t   m_historyHead;   /* write position */

    /* ── Cycle tracking ── */
    uint32_t m_cycleCount;
    float    m_cumulativeCapacity;   /* mAh accumulated over cycles */
    float    m_lastCapacity;         /* last full charge capacity */
    enum Phase { PHASE_IDLE, PHASE_CHARGE, PHASE_DISCHARGE };
    Phase    m_phase;

    /* ── Latest parsed values ── */
    float m_voltage;
    float m_current;
    float m_temperature;
    float m_pressure;

    /* ── Storage simulation ── */
    uint64_t m_storageBytesUsed;
};

#endif /* HMI_DATA_UART_PROVIDER_H */
