/*===========================================================================
 * DemoDataProvider.cpp — Synthetic battery data generator implementation
 *===========================================================================*/
#include "DemoDataProvider.h"
#include "config/AppConfig.h"
#include <cmath>
#include <cstring>

DemoDataProvider::DemoDataProvider(unsigned int seed)
    : m_rng(seed)
    , m_simulatedSoh(1.0f)
    , m_temperature(28.0f)
    , m_voltage(3.2f)
    , m_current(0.0f)
    , m_cycleCount(0)
    , m_capacity(BATTERY_NOMINAL_MAH)
    , m_tick(0)
    , m_storageBytesUsed(10ULL * 1024 * 1024 * 1024)  /* 10 GB used */
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
    m_simulatedSoh = 1.0f;
    m_temperature = 28.0f;
    m_voltage = 3.2f;
    m_current = 0.0f;
    m_cycleCount = 0;
    m_capacity = BATTERY_NOMINAL_MAH;
    m_tick = 0;
    m_storageBytesUsed = 10ULL * 1024 * 1024 * 1024;
    std::memset(&m_latest, 0, sizeof(m_latest));
}

void DemoDataProvider::generateSample()
{
    /* ── SOH: slow linear decay across test lifecycle ── */
    float soh = m_simulatedSoh;
    if (m_useFixedSoh) {
        soh = m_fixedSoh;
    } else if (m_autoDecay) {
        /* Decay 0.2% every 10 seconds (at 10Hz: every 100 ticks) */
        if (m_tick > 0 && (m_tick % 100) == 0) {
            m_simulatedSoh -= 0.002f;
            if (m_simulatedSoh < 0.15f) m_simulatedSoh = 0.15f;
        }
        soh = m_simulatedSoh;
    }

    /* ── IC curve: Gaussian peak + noise, modulated by SOH ── */
    std::normal_distribution<float> noise(0.0f, 0.03f);
    std::normal_distribution<float> tempWalk(0.0f, 0.05f);

    float peakHeight = 2.8f * soh;
    float peakPos = 64.0f + (1.0f - soh) * 8.0f;  /* peak drifts right as battery ages */
    float peakWidth = 18.0f + (1.0f - soh) * 2.0f;

    for (int i = 0; i < 128; i++) {
        float x = (static_cast<float>(i) - peakPos) / peakWidth;
        float ic_val = peakHeight * std::exp(-x * x * 0.5f)
                     + 0.1f * std::sin(x * 4.0f)   /* minor oscillation */
                     + noise(m_rng);
        m_latest.ic_curve[i] = ic_val;
    }

    /* ── Features vector (for PINN) ── */
    /* IC curve (indices 0-127) */
    std::memcpy(m_latest.features, m_latest.ic_curve, 128 * sizeof(float));

    /* Temperature (index 128): random walk 25-45°C */
    m_temperature += tempWalk(m_rng);
    if (m_temperature > 45.0f) m_temperature = 45.0f;
    if (m_temperature < 25.0f) m_temperature = 25.0f;
    m_latest.features[128] = m_temperature;

    /* log10(cycle_count + 1) (index 129) */
    m_latest.features[129] = std::log10(static_cast<float>(m_cycleCount) + 1.0f);

    /* dV proxy (index 130): start voltage drop */
    m_latest.features[130] = -0.03f - (1.0f - soh) * 0.08f + noise(m_rng) * 0.001f;

    /* Capacity (index 131) */
    m_latest.features[131] = m_capacity / BATTERY_NOMINAL_MAH;

    /* ── Scalar telemetry ── */
    m_latest.temperature = m_temperature;

    /* Voltage: charge ramp simulation
     *   Each charge cycle: 2.5V → 3.65V over ~30 seconds (300 ticks at 10Hz)
     *   Each discharge: 3.65V → 2.5V over ~30 seconds
     */
    uint32_t phase = m_tick % 600;  /* 60-second full cycle */
    if (phase < 300) {
        /* Charging */
        m_latest.voltage = BATTERY_CUTOFF_V
                         + (BATTERY_CHARGE_V - BATTERY_CUTOFF_V)
                         * static_cast<float>(phase) / 300.0f;
    } else {
        /* Discharging */
        m_latest.voltage = BATTERY_CHARGE_V
                         - (BATTERY_CHARGE_V - BATTERY_CUTOFF_V)
                         * static_cast<float>(phase - 300) / 300.0f;
    }
    /* Add noise */
    m_latest.voltage += noise(m_rng) * 0.02f;

    /* Current: charge at +50A, discharge at -50A */
    std::normal_distribution<float> currentNoise(0.0f, 2.0f);
    if (phase < 300) {
        m_latest.current = 50.0f + currentNoise(m_rng);
    } else {
        m_latest.current = -50.0f + currentNoise(m_rng);
    }

    /* Cycle count: increment every full charge-discharge pair */
    if (m_tick > 0 && (m_tick % 600) == 0) {
        m_cycleCount++;
    }
    m_latest.cycle_count = m_cycleCount;

    /* Capacity: decays with SOH */
    m_capacity = BATTERY_NOMINAL_MAH * (0.85f + 0.15f * soh);
    m_latest.capacity_mah = m_capacity;

    /* Cell swelling: occasional random spike (~2% probability per tick) */
    std::uniform_real_distribution<float> swellRng(0.0f, 1.0f);
    if (swellRng(m_rng) < 0.02f) {
        std::uniform_real_distribution<float> swellMag(0.1f, 0.6f);
        m_latest.cell_swelling = swellMag(m_rng);
    } else {
        m_latest.cell_swelling = std::max(0.0f, m_latest.cell_swelling - 0.05f);
    }

    /* Timestamp */
    m_latest.timestamp_ms = static_cast<int64_t>(m_tick) * DATA_ACQUISITION_MS;

    /* ── Storage: slow growth (~1KB per sample) ── */
    m_storageBytesUsed += 1024;
    if (m_storageBytesUsed > static_cast<uint64_t>(NAND_TOTAL_GB * 1024.0f * 1024.0f * 1024.0f))
        m_storageBytesUsed = static_cast<uint64_t>(NAND_TOTAL_GB * 1024.0f * 1024.0f * 1024.0f);

    m_tick++;
}

bool DemoDataProvider::read(BatterySample &sample)
{
    generateSample();
    sample = m_latest;
    return true;
}

bool DemoDataProvider::readStorage(StorageInfo &info)
{
    uint64_t total = static_cast<uint64_t>(NAND_TOTAL_GB * 1024.0f * 1024.0f * 1024.0f);
    info.total_bytes = total;
    info.used_bytes = m_storageBytesUsed;
    info.free_bytes = total - m_storageBytesUsed;
    info.usage_percent = 100.0f * static_cast<float>(m_storageBytesUsed) / static_cast<float>(total);
    info.bad_blocks = static_cast<uint32_t>(m_tick / 5000);  /* slowly growing bad blocks */
    info.total_blocks = 4096;
    return true;
}
