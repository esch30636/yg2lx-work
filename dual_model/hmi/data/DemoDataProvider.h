/*===========================================================================
 * DemoDataProvider.h — Physics-based charge/discharge simulator for HMI testing
 *
 * Models a real RA8-driven battery test system:
 *   - LiFePO4, 7200mAh (7.2Ah) nominal
 *   - CC charge @ 0.05A, cutoff at 3.60V
 *   - CC discharge @ 0.05A, cutoff at 2.80V
 *   - OCV(SOC) lookup table with characteristic LFP flat plateau
 *   - Internal resistance + polarization voltage (RC model)
 *   - EMA voltage filtering (α=0.02) with rate limiting (±0.002V/step)
 *   - ADC simulation (16-bit, Vref=3.3V, divider=3×, current shunt offset=1.489V)
 *
 * Cycle state machine: CHARGE → REST → DISCHARGE → REST → CHARGE ...
 * Timeline is highly accelerated for demo (~60s per full cycle).
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
    QString name() const override { return "Demo (RA8 CC-CV Sim)"; }
    void reset() override;

    /** Set simulated SOH level (0.0-1.0) for fixed-point testing */
    void setFixedSoh(float soh);
    /** Enable/disable SOH decay over time */
    void setAutoDecay(bool enable) { m_autoDecay = enable; }

private:
    enum CyclePhase {
        PHASE_CHARGE,
        PHASE_REST_AFTER_CHARGE,
        PHASE_DISCHARGE,
        PHASE_REST_AFTER_DISCHARGE
    };

    /** LiFePO4 OCV as a function of SOC [0,1] → voltage */
    float ocv(float soc) const;

    /** Generate one sample frame */
    void generateSample();

    /* ── RNG ── */
    std::mt19937 m_rng;

    /* ── Cycle state machine ── */
    CyclePhase m_phase;
    int        m_phaseTick;         /* ticks elapsed within current phase */
    float      m_demoSoc;           /* accelerated demo SOC [0, 1] */
    float      m_polarizationV;     /* Vp state variable (RC model) */

    /* ── Filter state ── */
    float      m_filteredVoltage;   /* EMA + rate-limited voltage */
    float      m_rawVoltage;        /* unfiltered terminal voltage */

    /* ── Aging ── */
    float      m_simulatedSoh;      /* current SOH (decays over time) */
    float      m_temperature;
    uint32_t   m_cycleCount;
    float      m_capacity;
    uint64_t   m_tick;
    uint64_t   m_storageBytesUsed;
    float      m_fixedSoh;
    bool       m_autoDecay;
    bool       m_useFixedSoh;

    /* Current sample cache */
    BatterySample m_latest;
};

#endif /* HMI_DEMO_DATA_PROVIDER_H */
