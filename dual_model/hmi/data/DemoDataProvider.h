/*===========================================================================
 * DemoDataProvider.h — Synthetic battery data generator for HMI testing
 *
 * Simulates a complete charge-discharge aging test:
 *   - IC curve with characteristic peak + noise modulated by simulated SOH
 *   - Voltage ramping between 2.5-3.65V (LiFePO4 charge cycle)
 *   - Current alternating charge/discharge at ±50A
 *   - Temperature random-walk 25-45°C
 *   - SOH linearly decaying across the test
 *   - Cell swelling: occasional random spikes
 *   - NAND storage: slow linear growth
 *===========================================================================*/
#ifndef HMI_DEMO_DATA_PROVIDER_H
#define HMI_DEMO_DATA_PROVIDER_H

#include "DataProvider.h"
#include <random>
#include <QString>

class DemoDataProvider : public DataProvider {
public:
    explicit DemoDataProvider(unsigned int seed = 42);

    bool read(BatterySample &sample) override;
    bool readStorage(StorageInfo &info) override;
    QString name() const override { return "Demo (Synthetic)"; }
    void reset() override;

    /** Set simulated SOH level (0.0-1.0) for fixed-point testing */
    void setFixedSoh(float soh);
    /** Enable/disable SOH decay over time */
    void setAutoDecay(bool enable) { m_autoDecay = enable; }

private:
    void generateSample();

    std::mt19937 m_rng;
    float m_simulatedSoh;       /* current simulated SOH (decays over time) */
    float m_temperature;        /* random walk */
    float m_voltage;            /* charge ramp */
    float m_current;            /* alternating */
    uint32_t m_cycleCount;
    float m_capacity;
    uint64_t m_tick;
    uint64_t m_storageBytesUsed;
    float m_fixedSoh;
    bool m_autoDecay;
    bool m_useFixedSoh;

    /* Current sample cache */
    BatterySample m_latest;
};

#endif /* HMI_DEMO_DATA_PROVIDER_H */
