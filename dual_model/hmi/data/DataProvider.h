/*===========================================================================
 * DataProvider.h — Abstract data source interface for Battery HMI
 *
 * Implementations:
 *   DemoDataProvider  — synthetic battery data for development/testing
 *   FileDataProvider  — binary file replay for verification
 *   (future) SpiDataProvider — RA8 SPI real-time data
 *===========================================================================*/
#ifndef HMI_DATA_PROVIDER_H
#define HMI_DATA_PROVIDER_H

#include <cstdint>
#include <cmath>
#include <QString>

/* ── Single acquisition sample ── */
struct BatterySample {
    float ic_curve[128];         /* raw IC curve (dQ/dV on 128-point voltage grid) */
    float features[132];         /* raw PINN feature vector (ic128 + temp + log_cycle + dV + cap) */
    float temperature;           /* °C */
    float voltage;               /* V */
    float current;               /* A (positive=charge, negative=discharge) */
    uint32_t cycle_count;        /* current cycle number */
    float capacity_mah;          /* measured capacity in mAh */
    float cell_swelling;         /* 0.0=normal, 1.0=severe swelling */
    int64_t timestamp_ms;        /* monotonic timestamp */
};

/* ── Storage status snapshot ── */
struct StorageInfo {
    float usage_percent;         /* 0.0 - 100.0 */
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    uint32_t bad_blocks;
    uint32_t total_blocks;
};

/* ── Alarm flags ── */
struct AlarmState {
    bool over_temp;
    bool over_voltage;
    bool over_current;
    bool cell_swelling;
    bool soh_critical;
    bool soh_warning;
    float over_temp_value;
    float over_voltage_value;
    float over_current_value;
    float cell_swelling_value;
    float soh_value;
    /* Cooldown timestamps (seconds) — prevent alarm flood */
    double over_temp_cooldown;
    double over_voltage_cooldown;
    double over_current_cooldown;
    double cell_swelling_cooldown;
    double soh_critical_cooldown;
    double soh_warning_cooldown;
};

/* Minimum interval between same-type alarm popups (seconds) */
#define ALARM_COOLDOWN_SEC 30.0

/* ── Abstract data provider ── */
class DataProvider {
public:
    virtual ~DataProvider() = default;

    /** Read next sample. Returns true if data was available. */
    virtual bool read(BatterySample &sample) = 0;

    /** Read storage info. Called less frequently. */
    virtual bool readStorage(StorageInfo &info) = 0;

    /** Human-readable provider name for UI display */
    virtual QString name() const = 0;

    /** Reset provider to initial state */
    virtual void reset() = 0;

    /**
     * Ensure features[132] is consistent with scalar telemetry fields.
     *
     * Some data sources (SPI RA8 firmware, binary file replay) may have
     * inconsistencies between features[128..131] and the separate scalar
     * fields (temperature, cycle_count, capacity_mah). This function
     * overwrites features[128..131] from the scalar fields — which are
     * the authoritative source — to guarantee correctness regardless
     * of data-source firmware version.
     *
     * Call this AFTER every successful read() before feeding features
     * to InferenceEngine::predictSOH().
     */
    static void fixFeatures(BatterySample &sample, float nominalVoltage, float nominalMah);
};

/* ── Inline: DataProvider::fixFeatures ── */
inline void DataProvider::fixFeatures(BatterySample &sample,
                                       float nominalVoltage,
                                       float nominalMah)
{
    /* features[0..127] = IC curve — copy from ic_curve field */
    for (int i = 0; i < 128; i++) {
        sample.features[i] = sample.ic_curve[i];
    }

    /* features[128] = temperature — use scalar field (authoritative) */
    sample.features[128] = sample.temperature;

    /* features[129] = log10(cycle_count + 1) */
    sample.features[129] = std::log10(static_cast<float>(sample.cycle_count) + 1.0f);

    /* features[130] = dV proxy — keep as-is from data source
     * (measured by RA8 ADC; we cannot reconstruct without raw ADC data) */

    /* features[131] = capacity (normalized to nominal) */
    sample.features[131] = (nominalMah > 0.0f)
        ? (sample.capacity_mah / nominalMah)
        : 0.0f;
    (void)nominalVoltage;  /* reserved for future use */
}

#endif /* HMI_DATA_PROVIDER_H */
