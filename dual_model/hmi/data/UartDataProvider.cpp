/*===========================================================================
 * UartDataProvider.cpp — RA8 real-time data via UART (implementation)
 *
 * Dependencies: POSIX termios (serial port configuration), no Qt networking.
 *
 * The serial port is opened in non-blocking mode (O_NONBLOCK). Each call to
 * read() drains all available lines, keeps the latest complete frame, and
 * fills a BatterySample. This avoids blocking the Qt event loop.
 *
 * Feature engineering pipeline:
 *   Raw (V,I) samples → ring buffer → charge/discharge detection
 *   → voltage binning → dQ = I*dt accumulation → dQ/dV per bin
 *   → 128-point IC curve → 132-dim feature vector
 *===========================================================================*/
#include "UartDataProvider.h"
#include "config/AppConfig.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

/* ── POSIX serial / I/O ── */
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/statvfs.h>
#include <errno.h>

/* ═══════════════════════════════════════════════════════════════════
 * Construction / Destruction
 * ═══════════════════════════════════════════════════════════════════ */

UartDataProvider::UartDataProvider(const Config &config)
    : m_config(config)
    , m_fd(-1)
    , m_sampleCount(0)
    , m_parseErrors(0)
    , m_tick(0)
    , m_history(nullptr)
    , m_historyCap(config.historySize)
    , m_historyLen(0)
    , m_historyHead(0)
    , m_cycleCount(0)
    , m_cumulativeCapacity(0.0f)
    , m_lastCapacity(BATTERY_NOMINAL_MAH)
    , m_phase(PHASE_IDLE)
    , m_voltage(0.0f)
    , m_current(0.0f)
    , m_temperature(25.0f)
    , m_pressure(0.0f)
    , m_storageBytesUsed(10ULL * 1024 * 1024 * 1024)
{
    /* Allocate history ring buffer */
    m_history = new RawSample[m_historyCap];
    memset(m_history, 0, m_historyCap * sizeof(RawSample));

    if (!uartOpen()) {
        fprintf(stderr, "[UartDataProvider] %s: %s\n",
                qPrintable(m_config.device), strerror(errno));
    } else {
        printf("[UartDataProvider] Opened %s @ %u baud, 8N1\n",
               qPrintable(m_config.device), m_config.baudRate);
    }
}

UartDataProvider::~UartDataProvider()
{
    uartClose();
    delete[] m_history;
    printf("[UartDataProvider] Shutdown: %u samples, %u parse errors\n",
           m_sampleCount, m_parseErrors);
}

/* ═══════════════════════════════════════════════════════════════════
 * Serial Port Management
 * ═══════════════════════════════════════════════════════════════════ */

bool UartDataProvider::uartOpen()
{
    const char *path = qPrintable(m_config.device);

    /* O_NONBLOCK: read() returns immediately if no data — essential for
     * not blocking the Qt event loop at 10 Hz. */
    m_fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0) return false;

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(m_fd, &tty) != 0) {
        close(m_fd);
        m_fd = -1;
        return false;
    }

    /* ── Baud rate ── */
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    /* ── 8N1 (8 data bits, no parity, 1 stop bit) ── */
    tty.c_cflag &= ~PARENB;          /* no parity */
    tty.c_cflag &= ~CSTOPB;          /* 1 stop bit */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;              /* 8 data bits */
    tty.c_cflag &= ~CRTSCTS;         /* no hardware flow control */
    tty.c_cflag |= CREAD | CLOCAL;   /* enable receiver, ignore modem lines */

    /* ── Raw mode: no canonical processing, no echo ── */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                     | INLCR | IGNCR | ICRNL | IXON);
    tty.c_oflag &= ~OPOST;           /* raw output */

    /* ── Read: return as soon as at least 1 byte is available ── */
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 0;

    /* Flush any stale data */
    tcflush(m_fd, TCIOFLUSH);

    if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
        close(m_fd);
        m_fd = -1;
        return false;
    }

    return true;
}

void UartDataProvider::uartClose()
{
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Line Reading (non-blocking, partial-line tolerant)
 *
 * Reads available bytes from the serial port and extracts complete
 * lines (terminated by '\n'). Incomplete trailing data is discarded
 * (it will be completed on the next read). Only the last complete
 * line is kept — this is correct for 10 Hz sampling where we only
 * need the most recent sample.
 * ═══════════════════════════════════════════════════════════════════ */

int UartDataProvider::uartReadLine(char *buf, size_t maxLen)
{
    if (m_fd < 0) return 0;

    char raw[256];
    ssize_t n = ::read(m_fd, raw, sizeof(raw) - 1);

    if (n <= 0) {
        /* EAGAIN = no data available (non-blocking), EIO = transient */
        return (n == 0 || errno == EAGAIN || errno == EIO) ? 0 : -1;
    }

    raw[n] = '\0';

    /* Find the last complete line (ends with '\n').
     * We only care about the most recent sample. */
    char *last_nl = nullptr;
    for (ssize_t i = n - 1; i >= 0; i--) {
        if (raw[i] == '\n') {
            last_nl = raw + i;
            break;
        }
    }

    if (last_nl == nullptr) {
        return 0;  /* incomplete line — wait for next read */
    }

    /* Find start of this line (previous '\n' or beginning of buffer) */
    char *line_start = raw;
    for (char *p = last_nl - 1; p >= raw; p--) {
        if (*p == '\n') {
            line_start = p + 1;
            break;
        }
    }

    size_t len = static_cast<size_t>(last_nl - line_start);
    if (len >= maxLen) len = maxLen - 1;

    memcpy(buf, line_start, len);
    buf[len] = '\0';

    /* Strip trailing '\r' if present (Windows-style line ending) */
    if (len > 0 && buf[len - 1] == '\r') {
        buf[len - 1] = '\0';
    }

    return static_cast<int>(len);
}

/* ═══════════════════════════════════════════════════════════════════
 * CSV Parsing
 *
 * Expected format:  voltage,current,temperature,pressure\n
 * Example:          3.652,1.234,25.6,1.201\n
 *
 * Returns true if all 4 fields were parsed successfully.
 * ═══════════════════════════════════════════════════════════════════ */

bool UartDataProvider::parseCsvLine(const char *line,
                                     float &voltage, float &current,
                                     float &temperature, float &pressure)
{
    /* Use sscanf with comma delimiter. %f handles both integer and
     * decimal formats, negative numbers, and scientific notation. */
    int n = sscanf(line, "%f,%f,%f,%f",
                   &voltage, &current, &temperature, &pressure);

    if (n != 4) {
        /* Only log every 100th parse error to avoid spam */
        if ((m_parseErrors % 100) == 0 && m_parseErrors > 0) {
            fprintf(stderr, "[UartDataProvider] Parse error #%u: "
                    "expected 4 fields, got %d. Line: '%.64s'\n",
                    m_parseErrors, n, line);
        }
        m_parseErrors++;
        return false;
    }

    /* ── Sanity checks — clamp obviously broken values ── */
    if (voltage < 0.0f || voltage > 10.0f) {
        m_parseErrors++;
        return false;
    }
    if (current < -200.0f || current > 200.0f) {
        m_parseErrors++;
        return false;
    }
    if (temperature < -50.0f || temperature > 150.0f) {
        m_parseErrors++;
        return false;
    }
    /* Pressure: 0–5V range (FSR sensor output) */
    if (pressure < 0.0f || pressure > 5.0f) {
        m_parseErrors++;
        return false;
    }

    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 * Feature Engineering — Raw Sample History
 * ═══════════════════════════════════════════════════════════════════ */

void UartDataProvider::pushHistory(float voltage, float current, float /*dt*/)
{
    m_history[m_historyHead].voltage = voltage;
    m_history[m_historyHead].current = current;
    m_historyHead = (m_historyHead + 1) % m_historyCap;
    if (m_historyLen < m_historyCap) {
        m_historyLen++;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * IC Curve Computation (Incremental Capacity — dQ/dV)
 *
 * Algorithm:
 *   1. Scan history buffer for consecutive (V, I) samples within the
 *      same charge or discharge phase.
 *   2. For each consecutive pair (V_k, I_k) → (V_{k+1}, I_{k+1}):
 *        dV = |V_{k+1} - V_k|
 *        dQ = |I_avg| * dt    (where I_avg = (I_k + I_{k+1}) / 2, dt = 0.1s)
 *        if dV > threshold: accumulate dQ into the voltage bin of V_avg
 *   3. IC_k = accumulated_dQ_k / bin_width  (gives dQ/dV per bin)
 *
 * The 128-point output spans CUTOFF_V to CHARGE_V (LiFePO4: 2.5V–3.65V).
 * ═══════════════════════════════════════════════════════════════════ */

void UartDataProvider::computeIcCurve(float *ic_out)
{
    const float dt = static_cast<float>(DATA_ACQUISITION_MS) / 1000.0f;  /* 0.1s */
    const float v_min = BATTERY_CUTOFF_V;    /* 2.5V */
    const float v_max = BATTERY_CHARGE_V;    /* 3.65V */
    const float bin_width = (v_max - v_min) / 128.0f;
    const float dv_threshold = 0.0005f;      /* min voltage change to count */

    /* Accumulated dQ per voltage bin */
    float *dQ_bins = new float[128];
    memset(dQ_bins, 0, 128 * sizeof(float));

    /* ── Integrate dQ across consecutive samples ── */
    if (m_historyLen < 2) {
        memset(ic_out, 0, 128 * sizeof(float));
        delete[] dQ_bins;
        return;
    }

    uint32_t count = m_historyLen;
    for (uint32_t i = 0; i < count - 1; i++) {
        /* Walk backwards from head (most recent data last) */
        uint32_t idx0 = (m_historyHead - count + i) % m_historyCap;
        uint32_t idx1 = (m_historyHead - count + i + 1) % m_historyCap;

        float v0 = m_history[idx0].voltage;
        float v1 = m_history[idx1].voltage;
        float i0 = m_history[idx0].current;
        float i1 = m_history[idx1].current;

        /* Only accumulate within the same phase (same current sign) */
        if ((i0 > 0.01f && i1 <= 0.01f) || (i0 < -0.01f && i1 >= -0.01f)) {
            continue;  /* phase change — skip this pair */
        }
        /* Skip idle/rest periods */
        if (fabsf(i0) < 0.01f || fabsf(i1) < 0.01f) {
            continue;
        }

        float dv = fabsf(v1 - v0);
        if (dv < dv_threshold) continue;

        float i_avg = fabsf((i0 + i1) / 2.0f);
        float dQ = i_avg * dt;          /* Ah = A × hours */
        float v_avg = (v0 + v1) / 2.0f;

        /* Bin index in [0, 127] */
        int bin = static_cast<int>((v_avg - v_min) / bin_width);
        if (bin < 0) bin = 0;
        if (bin >= 128) bin = 127;

        dQ_bins[bin] += dQ;
    }

    /* ── Convert dQ to dQ/dV ── */
    float max_ic = 0.0f;
    for (int i = 0; i < 128; i++) {
        ic_out[i] = dQ_bins[i] / bin_width;   /* Ah/V — IC unit */
        if (ic_out[i] > max_ic) max_ic = ic_out[i];
    }

    /* ── Normalize to reasonable range (similar to DemoDataProvider scale) ──
     * Target: peak ~3–9 for healthy cell, ~1–3 for aged cell.
     * Raw IC from Ah/V is very small (~0.01-0.1). Scale up for model input. */
    if (max_ic > 0.0f) {
        float scale = 8.0f / max_ic;   /* normalize peak to ~8 */
        for (int i = 0; i < 128; i++) {
            ic_out[i] *= scale;
        }
    }

    /* ── If no data accumulated, fill with zeros (first few seconds) ── */
    delete[] dQ_bins;
}

/* ═══════════════════════════════════════════════════════════════════
 * Cycle Boundary Detection
 *
 * Detects charge→discharge and discharge→charge transitions to
 * increment cycle count and estimate capacity.
 * ═══════════════════════════════════════════════════════════════════ */

void UartDataProvider::detectCycleBoundary()
{
    /* Simple hysteresis: current crosses +0.05A → charging,
     * crosses -0.05A → discharging.
     * A full cycle = charge + discharge pair. */

    Phase newPhase;
    if (m_current > 0.05f) {
        newPhase = PHASE_CHARGE;
    } else if (m_current < -0.05f) {
        newPhase = PHASE_DISCHARGE;
    } else {
        newPhase = m_phase;  /* idle — keep current phase */
    }

    /* Charge → Discharge transition: a charge phase just ended.
     * Increment cycle count and estimate capacity. */
    if (m_phase == PHASE_CHARGE && newPhase == PHASE_DISCHARGE) {
        m_cycleCount++;
        /* Estimate capacity from cumulative charge input.
         * In a real system this would be the integrated I*dt over
         * the charge phase. Here we use a simple decay model. */
        m_lastCapacity = BATTERY_NOMINAL_MAH * (0.85f + 0.15f
                         * (1.0f - static_cast<float>(m_cycleCount) * 0.0003f));
        if (m_lastCapacity < BATTERY_NOMINAL_MAH * 0.65f) {
            m_lastCapacity = BATTERY_NOMINAL_MAH * 0.65f;
        }
        printf("[UartDataProvider] Cycle %u complete — est. capacity: %.0f mAh\n",
               m_cycleCount, m_lastCapacity);
    }

    m_phase = newPhase;
}

/* ═══════════════════════════════════════════════════════════════════
 * Feature Vector Assembly
 *
 * Builds the 132-dim feature vector from current state:
 *   features[0..127]   = IC curve
 *   features[128]       = temperature
 *   features[129]       = log10(cycle_count + 1)
 *   features[130]       = dV proxy (negative for aging cells)
 *   features[131]       = normalized capacity
 * ═══════════════════════════════════════════════════════════════════ */

void UartDataProvider::fillFeatures(BatterySample &sample)
{
    /* ── IC curve ── */
    computeIcCurve(sample.ic_curve);

    /* ── features[0..127] = IC curve copy ── */
    memcpy(sample.features, sample.ic_curve, 128 * sizeof(float));

    /* ── features[128] = temperature (authoritative scalar) ── */
    sample.features[128] = m_temperature;

    /* ── features[129] = log10(cycle_count + 1) ── */
    sample.features[129] = log10f(static_cast<float>(m_cycleCount) + 1.0f);

    /* ── features[130] = dV proxy (decreases as battery ages) ── */
    float soh_est = m_lastCapacity / BATTERY_NOMINAL_MAH;
    sample.features[130] = -0.03f - (1.0f - soh_est) * 0.08f;

    /* ── features[131] = normalized capacity ── */
    sample.features[131] = (BATTERY_NOMINAL_MAH > 0.0f)
                           ? (m_lastCapacity / BATTERY_NOMINAL_MAH)
                           : 0.0f;
}

/* ═══════════════════════════════════════════════════════════════════
 * DataProvider Interface
 * ═══════════════════════════════════════════════════════════════════ */

bool UartDataProvider::read(BatterySample &sample)
{
    if (m_fd < 0) {
        /* Serial port not available — return zeros */
        memset(&sample, 0, sizeof(sample));
        return false;
    }

    /* ── Drain all available lines, keep the last complete one ── */
    char line[128];
    bool gotLine = false;

    while (true) {
        int len = uartReadLine(line, sizeof(line));
        if (len <= 0) break;

        float v, i, t, p;
        if (parseCsvLine(line, v, i, t, p)) {
            m_voltage     = v;
            m_current     = i;
            m_temperature = t;
            m_pressure    = p;
            gotLine = true;
        }
    }

    if (!gotLine) {
        /* No new data — return false, caller will retry next tick */
        return false;
    }

    /* ── Populate BatterySample ── */
    memset(&sample, 0, sizeof(sample));

    sample.voltage       = m_voltage;
    sample.current       = m_current;
    sample.temperature   = m_temperature;
    sample.cell_swelling = (m_pressure > 2.0f) ? (m_pressure - 2.0f) / 3.0f : 0.0f;
    sample.cycle_count   = m_cycleCount;
    sample.capacity_mah  = m_lastCapacity;
    sample.timestamp_ms  = static_cast<int64_t>(m_tick)
                           * static_cast<int64_t>(DATA_ACQUISITION_MS);

    /* ── Feature engineering ── */
    pushHistory(m_voltage, m_current, static_cast<float>(DATA_ACQUISITION_MS) / 1000.0f);
    detectCycleBoundary();
    fillFeatures(sample);

    /* ── Storage growth (simulated) ── */
    m_storageBytesUsed += 1024;
    uint64_t maxBytes = static_cast<uint64_t>(NAND_TOTAL_GB * 1024.0 * 1024.0 * 1024.0);
    if (m_storageBytesUsed > maxBytes) m_storageBytesUsed = maxBytes;

    m_sampleCount++;
    m_tick++;

    return true;
}

bool UartDataProvider::readStorage(StorageInfo &info)
{
    /* Try local statvfs first, fall back to simulated */
    static const char *mountPoints[] = { "/data", "/mnt/nand", "/home", "/", nullptr };
    struct statvfs st;
    bool ok = false;

    for (const char **mp = mountPoints; *mp != nullptr; mp++) {
        if (statvfs(*mp, &st) == 0) {
            ok = true;
            break;
        }
    }

    if (ok) {
        uint64_t total = st.f_blocks * st.f_frsize;
        uint64_t avail = st.f_bavail * st.f_frsize;
        uint64_t used  = total - avail;

        info.total_bytes    = total;
        info.used_bytes     = used;
        info.free_bytes     = avail;
        info.usage_percent  = (total > 0) ? 100.0f * static_cast<float>(used) / static_cast<float>(total) : 0.0f;
        info.bad_blocks     = 0;
        info.total_blocks   = static_cast<uint32_t>(st.f_blocks);
    } else {
        uint64_t total = static_cast<uint64_t>(NAND_TOTAL_GB * 1024.0f * 1024.0f * 1024.0f);
        info.total_bytes    = total;
        info.used_bytes     = m_storageBytesUsed;
        info.free_bytes     = total - m_storageBytesUsed;
        info.usage_percent  = 100.0f * static_cast<float>(m_storageBytesUsed) / static_cast<float>(total);
        info.bad_blocks     = 0;
        info.total_blocks   = 4096;
    }

    return true;
}

QString UartDataProvider::name() const
{
    if (m_fd >= 0) {
        return QString("UART/RA8 (%1)").arg(m_config.device);
    }
    return QString("UART/RA8 (%1 — 未连接)").arg(m_config.device);
}

void UartDataProvider::reset()
{
    m_sampleCount = 0;
    m_parseErrors = 0;
    m_tick        = 0;
    m_historyLen  = 0;
    m_historyHead = 0;
    m_cycleCount  = 0;
    m_phase       = PHASE_IDLE;
    m_lastCapacity = BATTERY_NOMINAL_MAH;
    m_storageBytesUsed = 10ULL * 1024 * 1024 * 1024;

    /* Flush serial buffers */
    if (m_fd >= 0) {
        tcflush(m_fd, TCIOFLUSH);
    }
}
