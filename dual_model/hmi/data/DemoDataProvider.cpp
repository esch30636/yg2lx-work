/*===========================================================================
 * DemoDataProvider.cpp — Physics-based charge/discharge simulator
 *
 * Models a real RA8-driven battery test system with:
 *   - LiFePO4 OCV(SOC) characteristic (flat plateau + sharp knees)
 *   - CC charge/discharge at 0.05A
 *   - Internal resistance + RC polarization model
 *   - EMA + rate-limited voltage filtering (matching RA8 firmware)
 *   - ADC quantization simulation
 *
 * Physical parameters:
 *   Battery:      LiFePO4, 7200mAh (7.2Ah), 3.2V nominal
 *   Charge:       CC 0.05A, cutoff 3.60V
 *   Discharge:    CC 0.05A, cutoff 2.80V
 *   R_int:        0.15Ω
 *   R_p:          0.05Ω, τ_p = 5.0s
 *   V_divider:    3× (ADC measures 1/3 of battery voltage)
 *   I_shunt_gain: 5A/V, offset 1.489V
 *   ADC:          16-bit, Vref=3.3V
 *   EMA filter:   α = 0.02, rate limit ±0.002V/step
 *
 * Timing (highly accelerated for demo):
 *   Charge:    30s (300 ticks @ 10Hz)
 *   Rest:      5s  (50 ticks)
 *   Discharge: 30s (300 ticks)
 *   Rest:      5s  (50 ticks)
 *   Full cycle: ~70 seconds
 *===========================================================================*/
#include "DemoDataProvider.h"
#include "config/AppConfig.h"
#include <cmath>
#include <cstring>
#include <algorithm>

/* ═══════════════════════════════════════════════════════════════════════════
 * Physical Constants — matching RA8 firmware parameters
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Battery */
static constexpr float BAT_CAPACITY_AH        = 7.2f;    /* 7200 mAh */
static constexpr float BAT_CHG_CURRENT_A      = 0.05f;   /* CC charge target */
static constexpr float BAT_DIS_CURRENT_A      = 0.05f;   /* CC discharge target */
static constexpr float BAT_RINT_OHM           = 0.15f;   /* internal resistance */
static constexpr float BAT_RP_OHM             = 0.05f;   /* polarization resistance */
static constexpr float BAT_TAU_P_SEC          = 5.0f;    /* polarization time constant */
static constexpr float BAT_V_CHG_CUTOFF       = 3.60f;   /* charge termination */
static constexpr float BAT_V_DIS_CUTOFF       = 2.80f;   /* discharge termination */

/* ADC simulation */
static constexpr float ADC_VREF               = 3.3f;
static constexpr float ADC_MAX_COUNT          = 65535.0f;
static constexpr float ADC_V_DIVIDER          = 3.0f;    /* voltage divider ratio */
static constexpr float ADC_CURRENT_OFFSET_V   = 1.489f;  /* zero-current ADC voltage */
static constexpr float ADC_CURRENT_SHUNT_GAIN = 5.0f;    /* A/V after shunt amp */

/* Voltage filter (matching RA8 firmware EMA + rate limit) */
static constexpr float EMA_ALPHA             = 0.02f;
static constexpr float VOLTAGE_RATE_LIMIT    = 0.002f;   /* max V change per 100ms step */

/* Demo cycle timing (10Hz = 100ms per tick) */
static constexpr int   DEMO_TICKS_CHARGE     = 300;      /* 30s charge */
static constexpr int   DEMO_TICKS_DISCHARGE  = 300;      /* 30s discharge */
static constexpr int   DEMO_TICKS_REST       = 50;       /* 5s rest */
static constexpr int   DEMO_TICKS_FULL_CYCLE = DEMO_TICKS_CHARGE + DEMO_TICKS_REST
                                              + DEMO_TICKS_DISCHARGE + DEMO_TICKS_REST;

/* SOH decay: 0.2% per full demo cycle (~70s) */
static constexpr float SOH_DECAY_PER_CYCLE   = 0.002f;

/* Temperature: random walk in LiFePO4 operating range */
static constexpr float TEMP_INITIAL          = 25.0f;
static constexpr float TEMP_MIN              = 20.0f;
static constexpr float TEMP_MAX              = 45.0f;

/* ═══════════════════════════════════════════════════════════════════════════
 * LiFePO4 OCV(SOC) Lookup Table
 *
 * Characteristic LFP shape:
 *   - Sharp knee at low SOC (0-5%): 2.80 → 3.20V
 *   - Very flat plateau (10-90%):  3.28 → 3.33V  (ΔV < 50mV!)
 *   - Sharp knee at high SOC (90-100%): 3.33 → 3.60V
 *
 * Table: 21 points at 5% SOC intervals, linear interpolation.
 * ═══════════════════════════════════════════════════════════════════════════ */
static constexpr int   OCV_TABLE_SIZE = 21;
static constexpr float OCV_TABLE_SOC[OCV_TABLE_SIZE] = {
    0.00f, 0.05f, 0.10f, 0.15f, 0.20f, 0.25f, 0.30f, 0.35f,
    0.40f, 0.45f, 0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f,
    0.80f, 0.85f, 0.90f, 0.95f, 1.00f
};
static constexpr float OCV_TABLE_V[OCV_TABLE_SIZE] = {
    2.80f,  /* 0%  — fully discharged */
    3.20f,  /* 5%  — sharp knee */
    3.28f,  /* 10% */
    3.29f,  /* 15% */
    3.30f,  /* 20% — entering plateau */
    3.31f,  /* 25% */
    3.31f,  /* 30% */
    3.31f,  /* 35% */
    3.32f,  /* 40% — plateau center */
    3.32f,  /* 45% */
    3.33f,  /* 50% */
    3.33f,  /* 55% */
    3.33f,  /* 60% */
    3.33f,  /* 65% */
    3.34f,  /* 70% */
    3.35f,  /* 75% */
    3.38f,  /* 80% — exiting plateau */
    3.42f,  /* 85% */
    3.48f,  /* 90% — sharp knee up */
    3.55f,  /* 95% */
    3.65f   /* 100% — fully charged (OCV, not terminal) */
};

float DemoDataProvider::ocv(float soc) const
{
    /* Clamp to table range */
    if (soc <= OCV_TABLE_SOC[0])             return OCV_TABLE_V[0];
    if (soc >= OCV_TABLE_SOC[OCV_TABLE_SIZE - 1]) return OCV_TABLE_V[OCV_TABLE_SIZE - 1];

    /* Linear interpolation */
    for (int i = 0; i < OCV_TABLE_SIZE - 1; i++) {
        if (soc >= OCV_TABLE_SOC[i] && soc < OCV_TABLE_SOC[i + 1]) {
            float t = (soc - OCV_TABLE_SOC[i])
                    / (OCV_TABLE_SOC[i + 1] - OCV_TABLE_SOC[i]);
            return OCV_TABLE_V[i] + t * (OCV_TABLE_V[i + 1] - OCV_TABLE_V[i]);
        }
    }
    return OCV_TABLE_V[OCV_TABLE_SIZE - 1];
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Constructor / Reset
 * ═══════════════════════════════════════════════════════════════════════════ */

DemoDataProvider::DemoDataProvider(unsigned int seed)
    : m_rng(seed)
    , m_phase(PHASE_CHARGE)
    , m_phaseTick(0)
    , m_demoSoc(0.05f)            /* start near discharged */
    , m_polarizationV(0.0f)
    , m_filteredVoltage(3.20f)    /* initial filtered voltage */
    , m_rawVoltage(3.20f)
    , m_simulatedSoh(1.0f)
    , m_temperature(TEMP_INITIAL)
    , m_cycleCount(0)
    , m_capacity(BATTERY_NOMINAL_MAH)
    , m_tick(0)
    , m_storageBytesUsed(10ULL * 1024 * 1024 * 1024)
    , m_fixedSoh(0.85f)
    , m_autoDecay(true)
    , m_useFixedSoh(false)
{
    std::memset(&m_latest, 0, sizeof(m_latest));
}

void DemoDataProvider::setFixedSoh(float soh)
{
    m_fixedSoh = std::max(0.0f, std::min(1.0f, soh));
    m_useFixedSoh = true;
}

void DemoDataProvider::reset()
{
    m_phase           = PHASE_CHARGE;
    m_phaseTick       = 0;
    m_demoSoc         = 0.05f;
    m_polarizationV   = 0.0f;
    m_filteredVoltage = 3.20f;
    m_rawVoltage      = 3.20f;
    m_simulatedSoh    = 1.0f;
    m_temperature     = TEMP_INITIAL;
    m_cycleCount      = 0;
    m_capacity        = BATTERY_NOMINAL_MAH;
    m_tick            = 0;
    m_storageBytesUsed = 10ULL * 1024 * 1024 * 1024;
    std::memset(&m_latest, 0, sizeof(m_latest));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Sample Generation — physics-based charge/discharge cycle
 * ═══════════════════════════════════════════════════════════════════════════ */

void DemoDataProvider::generateSample()
{
    /* ── SOH (aging) ── */
    float soh = m_simulatedSoh;
    if (m_useFixedSoh) {
        soh = m_fixedSoh;
    } else if (m_autoDecay && m_tick > 0 && (m_tick % DEMO_TICKS_FULL_CYCLE) == 0) {
        m_simulatedSoh -= SOH_DECAY_PER_CYCLE;
        if (m_simulatedSoh < 0.15f) m_simulatedSoh = 0.15f;
        soh = m_simulatedSoh;
    }

    /* ── Determine cycle phase ── */
    int cycleTick = m_phaseTick;

    /* Phase transition logic */
    switch (m_phase) {
    case PHASE_CHARGE:
        if (cycleTick >= DEMO_TICKS_CHARGE) {
            m_phase = PHASE_REST_AFTER_CHARGE;
            m_phaseTick = 0;
            cycleTick = 0;
        }
        break;
    case PHASE_REST_AFTER_CHARGE:
        if (cycleTick >= DEMO_TICKS_REST) {
            m_phase = PHASE_DISCHARGE;
            m_phaseTick = 0;
            cycleTick = 0;
        }
        break;
    case PHASE_DISCHARGE:
        if (cycleTick >= DEMO_TICKS_DISCHARGE) {
            m_phase = PHASE_REST_AFTER_DISCHARGE;
            m_phaseTick = 0;
            cycleTick = 0;
            m_cycleCount++;  /* full charge-discharge pair completed */
        }
        break;
    case PHASE_REST_AFTER_DISCHARGE:
        if (cycleTick >= DEMO_TICKS_REST) {
            m_phase = PHASE_CHARGE;
            m_phaseTick = 0;
            cycleTick = 0;
        }
        break;
    }

    /* ── Current ── */
    float targetCurrent = 0.0f;
    float socStart, socEnd;
    int phaseDuration;

    switch (m_phase) {
    case PHASE_CHARGE:
        targetCurrent  = +BAT_CHG_CURRENT_A;
        socStart       = 0.05f;  /* start charging from near-empty */
        socEnd         = 1.00f;
        phaseDuration  = DEMO_TICKS_CHARGE;
        break;
    case PHASE_DISCHARGE:
        targetCurrent  = -BAT_DIS_CURRENT_A;  /* negative = discharge */
        socStart       = 0.95f;  /* start discharging from near-full */
        socEnd         = 0.05f;
        phaseDuration  = DEMO_TICKS_DISCHARGE;
        break;
    default: /* REST phases */
        targetCurrent  = 0.0f;
        socStart       = m_demoSoc;
        socEnd         = m_demoSoc;
        phaseDuration  = DEMO_TICKS_REST;
        break;
    }

    /* ── Current with PID-like ramp-up (exponential approach) ── */
    /* τ_i = phaseDuration * 0.1 (~3s charge, ~3s discharge) */
    float tauI = static_cast<float>(phaseDuration) * 0.1f;
    float tNorm = static_cast<float>(cycleTick) / tauI;
    float rampFactor = 1.0f - std::exp(-tNorm);
    if (rampFactor > 1.0f) rampFactor = 1.0f;

    /* Add ADC quantization noise to current */
    std::normal_distribution<float> currentNoise(0.0f, 0.002f);  /* ~2mA RMS noise */
    float current = targetCurrent * rampFactor + currentNoise(m_rng);

    /* ── SOC: linear ramp within phase ── */
    if (m_phase == PHASE_CHARGE || m_phase == PHASE_DISCHARGE) {
        float frac = static_cast<float>(cycleTick) / static_cast<float>(phaseDuration);
        if (frac > 1.0f) frac = 1.0f;
        m_demoSoc = socStart + frac * (socEnd - socStart);
    }
    /* REST: SOC stays constant */

    /* ── Terminal voltage: OCV(SOC) + IR + polarization ── */
    float ocvVal = ocv(m_demoSoc);
    float irDrop = current * BAT_RINT_OHM;

    /* RC polarization model: Vp[k+1] = Vp[k] + (dt/τp) * (Rp*I - Vp[k]) */
    float dt = static_cast<float>(DATA_ACQUISITION_MS) / 1000.0f;  /* 0.1s */
    float alphaP = dt / BAT_TAU_P_SEC;  /* 0.1 / 5.0 = 0.02 */
    float vpTarget = BAT_RP_OHM * current;
    m_polarizationV += alphaP * (vpTarget - m_polarizationV);

    /* Raw terminal voltage (before filtering) */
    m_rawVoltage = ocvVal + irDrop + m_polarizationV;

    /* Add small measurement noise (~±2mV) */
    std::normal_distribution<float> voltNoise(0.0f, 0.002f);
    m_rawVoltage += voltNoise(m_rng);

    /* ── EMA filter: Vf[k] = Vf[k-1] + α * (Vraw[k] - Vf[k-1]) ── */
    float emaStep = EMA_ALPHA * (m_rawVoltage - m_filteredVoltage);

    /* Rate limit: |ΔV| ≤ 0.002V per step */
    if (emaStep >  VOLTAGE_RATE_LIMIT) emaStep =  VOLTAGE_RATE_LIMIT;
    if (emaStep < -VOLTAGE_RATE_LIMIT) emaStep = -VOLTAGE_RATE_LIMIT;

    m_filteredVoltage += emaStep;

    /* The display voltage = filtered value (what the HMI actually sees) */
    float displayVoltage = m_filteredVoltage;

    /* Clamp to physical range */
    if (displayVoltage < 2.0f) displayVoltage = 2.0f;
    if (displayVoltage > 4.2f) displayVoltage = 4.2f;

    /* ── IC curve: LFP double-peak model (for PINN feature vector) ── */
    std::normal_distribution<float> icNoise(0.0f, 0.04f);

    /* Main peak (FePO₄/LiFePO₄ phase transition): sharp, near 3.40V */
    float mainHeight = 9.0f * soh + 1.0f;
    float mainSigma  = 1.5f + (1.0f - soh) * 2.0f;
    float mainPos    = 99.0f + (1.0f - soh) * 3.0f;    /* drifts right ~27mV with age */

    /* Secondary peak (solid-solution region): near 3.33V */
    float secHeight  = 2.5f * soh + 0.3f;
    float secSigma   = 3.0f + (1.0f - soh) * 3.0f;
    float secPos     = 92.0f + (1.0f - soh) * 3.0f;

    for (int i = 0; i < 128; i++) {
        float dx_main = (static_cast<float>(i) - mainPos) / mainSigma;
        float dx_sec  = (static_cast<float>(i) - secPos)  / secSigma;
        float ic_val  = mainHeight * std::exp(-dx_main * dx_main * 0.5f)
                      + secHeight  * std::exp(-dx_sec  * dx_sec  * 0.5f)
                      + 0.08f * std::sin(static_cast<float>(i) * 0.25f)
                      + 0.05f
                      + icNoise(m_rng);
        if (ic_val < 0.0f) ic_val = 0.0f;
        m_latest.ic_curve[i] = ic_val;
    }

    /* ── Features vector [132] for PINN ── */
    std::memcpy(m_latest.features, m_latest.ic_curve, 128 * sizeof(float));

    /* Temperature: random walk */
    std::normal_distribution<float> tempWalk(0.0f, 0.03f);
    m_temperature += tempWalk(m_rng);
    if (m_temperature > TEMP_MAX) m_temperature = TEMP_MAX;
    if (m_temperature < TEMP_MIN) m_temperature = TEMP_MIN;
    m_latest.features[128] = m_temperature;

    /* log10(cycle_count + 1) */
    m_latest.features[129] = std::log10(static_cast<float>(m_cycleCount) + 1.0f);

    /* dV proxy */
    m_latest.features[130] = -0.03f - (1.0f - soh) * 0.08f + icNoise(m_rng) * 0.001f;

    /* Capacity (normalized) */
    m_latest.features[131] = m_capacity / BATTERY_NOMINAL_MAH;

    /* ── Scalar telemetry ── */
    m_latest.temperature = m_temperature;
    m_latest.voltage     = displayVoltage;
    m_latest.current     = current;
    m_latest.cycle_count = m_cycleCount;

    /* Capacity decays with SOH */
    m_capacity = BATTERY_NOMINAL_MAH * (0.85f + 0.15f * soh);
    m_latest.capacity_mah = m_capacity;

    /* Cell swelling: rare spikes (~1% per tick, self-decaying) */
    std::uniform_real_distribution<float> swellRng(0.0f, 1.0f);
    if (swellRng(m_rng) < 0.01f) {
        std::uniform_real_distribution<float> swellMag(0.1f, 0.5f);
        m_latest.cell_swelling = swellMag(m_rng);
    } else {
        m_latest.cell_swelling = std::max(0.0f, m_latest.cell_swelling - 0.03f);
    }

    m_latest.timestamp_ms = static_cast<int64_t>(m_tick) * DATA_ACQUISITION_MS;

    /* ── Storage: slow growth ── */
    m_storageBytesUsed += 1024;
    uint64_t totalStorage = static_cast<uint64_t>(NAND_TOTAL_GB * 1024.0f * 1024.0f * 1024.0f);
    if (m_storageBytesUsed > totalStorage)
        m_storageBytesUsed = totalStorage;

    m_tick++;
    m_phaseTick++;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DataProvider interface
 * ═══════════════════════════════════════════════════════════════════════════ */

bool DemoDataProvider::read(BatterySample &sample)
{
    generateSample();
    sample = m_latest;
    return true;
}

bool DemoDataProvider::readStorage(StorageInfo &info)
{
    uint64_t total = static_cast<uint64_t>(NAND_TOTAL_GB * 1024.0f * 1024.0f * 1024.0f);
    info.total_bytes   = total;
    info.used_bytes    = m_storageBytesUsed;
    info.free_bytes    = total - m_storageBytesUsed;
    info.usage_percent = 100.0f * static_cast<float>(m_storageBytesUsed) / static_cast<float>(total);
    info.bad_blocks    = static_cast<uint32_t>(m_tick / 5000);
    info.total_blocks  = 4096;
    return true;
}
