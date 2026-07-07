/*===========================================================================
 * SohAccumulator.h — Sliding-window SOH accumulator with convergence detection
 *
 * Maintains a fixed-size deque of PINN inference results. Computes
 * median, mean, stddev, and 95% confidence intervals on demand.
 *
 * Convergence detection: monitors stddev over consecutive windows.
 * When stddev < epsilon for stable_checks consecutive windows, the
 * SOH estimate has converged and PINN inference can stop.
 *
 * Thread safety: not thread-safe — designed for single-thread use
 * (main thread via QTimer).
 *===========================================================================*/
#ifndef HMI_INFERENCE_SOH_ACCUMULATOR_H
#define HMI_INFERENCE_SOH_ACCUMULATOR_H

#include <deque>
#include <vector>
#include <algorithm>
#include <cmath>

class SohAccumulator {
public:
    struct Stats {
        float median;
        float mean;
        float stddev;
        float min;
        float max;
        float ci_95_half;       /* 1.96 * stddev / sqrt(n) */
        int   sample_count;
        bool  window_full;       /* true once m_maxSize samples collected */
    };

    struct ConvergenceStatus {
        bool converged;          /* true when convergence criteria met */
        float final_soh;         /* median SOH at convergence */
        float final_ci_half;     /* CI half-width at convergence */
        float current_stddev;    /* most recent stddev */
        int   samples_used;      /* total samples pushed */
        int   stable_count;      /* consecutive windows under epsilon */
    };

    explicit SohAccumulator(int maxSize = 1200)
        : m_maxSize(maxSize)
        , m_totalSamples(0)
        , m_stableCount(0)
    {
    }

    /** Push one PINN inference result. NaN/Inf values are silently dropped. */
    void push(float soh)
    {
        if (std::isnan(soh) || std::isinf(soh))
            return;

        if (static_cast<int>(m_window.size()) >= m_maxSize)
            m_window.pop_front();

        m_window.push_back(soh);
        m_totalSamples++;
    }

    /** Compute statistics over the current window (O(n log n) for median). */
    Stats stats() const
    {
        Stats s;
        const int n = static_cast<int>(m_window.size());

        s.sample_count = n;
        s.window_full  = (n >= m_maxSize);

        if (n == 0) {
            s.median = s.mean = s.stddev = s.min = s.max = s.ci_95_half = 0.0f;
            return s;
        }

        /* ── Min, max, mean, variance (single pass) ── */
        float sum = 0.0f, sum_sq = 0.0f;
        s.min = m_window[0];
        s.max = m_window[0];

        for (int i = 0; i < n; i++) {
            float v = m_window[static_cast<size_t>(i)];
            sum   += v;
            sum_sq += v * v;
            if (v < s.min) s.min = v;
            if (v > s.max) s.max = v;
        }

        s.mean   = sum / static_cast<float>(n);
        float variance = (sum_sq / static_cast<float>(n)) - (s.mean * s.mean);
        if (variance < 0.0f) variance = 0.0f;   /* clamp fp rounding */
        s.stddev = std::sqrt(variance);

        /* ── Median (sort copy) ── */
        std::vector<float> sorted(m_window.begin(), m_window.end());
        std::sort(sorted.begin(), sorted.end());

        if (n % 2 == 1) {
            s.median = sorted[n / 2];
        } else {
            s.median = (sorted[n / 2 - 1] + sorted[n / 2]) * 0.5f;
        }

        /* ── 95% confidence interval half-width ── */
        if (n > 1 && s.stddev > 0.0f) {
            s.ci_95_half = 1.96f * s.stddev / std::sqrt(static_cast<float>(n));
        } else {
            s.ci_95_half = 0.0f;
        }

        return s;
    }

    /**
     * Check convergence: stddev < epsilon for stable_required consecutive
     * windows, with at least min_samples collected.
     *
     * Call this AFTER push() — it operates on the current window state.
     * Updates the internal stable-count state machine.
     */
    ConvergenceStatus checkConvergence(
        float epsilon = 0.005f,
        int   min_samples = 60,
        int   stable_required = 3)
    {
        ConvergenceStatus cs;
        const int n = static_cast<int>(m_window.size());

        Stats s = stats();
        cs.current_stddev = s.stddev;
        cs.samples_used   = m_totalSamples;
        cs.final_soh      = s.median;
        cs.final_ci_half  = s.ci_95_half;

        /* Must have minimum sample count to consider convergence */
        if (n < min_samples) {
            cs.converged    = false;
            cs.stable_count = 0;
            m_stableCount   = 0;
            return cs;
        }

        /* Check if this window is under the epsilon threshold */
        if (s.stddev < epsilon && s.stddev > 0.0f) {
            m_stableCount++;
        } else {
            m_stableCount = 0;
        }

        cs.stable_count = m_stableCount;
        cs.converged    = (m_stableCount >= stable_required);

        return cs;
    }

    void reset()
    {
        m_window.clear();
        m_totalSamples = 0;
        m_stableCount  = 0;
    }

    int count() const { return static_cast<int>(m_window.size()); }
    bool isWindowFull() const { return count() >= m_maxSize; }

private:
    std::deque<float> m_window;
    int m_maxSize;
    int m_totalSamples;
    int m_stableCount;
};

#endif /* HMI_INFERENCE_SOH_ACCUMULATOR_H */
