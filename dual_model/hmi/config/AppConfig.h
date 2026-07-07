/*===========================================================================
 * AppConfig.h — Central compile-time constants for Battery HMI
 *
 * Controls: refresh intervals, alarm thresholds, chart window, layout sizing
 *===========================================================================*/
#ifndef HMI_APPCONFIG_H
#define HMI_APPCONFIG_H

/* ── Window Geometry (640×480) ── */
#define WINDOW_WIDTH          640
#define WINDOW_HEIGHT         480
#define HEADER_HEIGHT         44
#define FOOTER_HEIGHT         0
#define STATUS_BAR_HEIGHT     24
#define IC_CHART_HEIGHT       220
#define HEALTH_BAR_HEIGHT     28
#define PROGRESS_BAR_HEIGHT   22

/* ── Refresh Intervals (milliseconds) ── */
#define DATA_ACQUISITION_MS   100    /* sensor read: 10 Hz */
#define PINN_INFERENCE_MS     500    /* SOH update: 2 Hz */
#define CNN_INFERENCE_MS      2000   /* Stage+RUL update: 0.5 Hz */
#define ALARM_CHECK_MS        200    /* threshold poll: 5 Hz */
#define CLOCK_UPDATE_MS       1000   /* header clock: 1 Hz */
#define INIT_PROGRESS_MS      30     /* progress bar animation: ~30 fps */

/* ── Init Screen ── */
#define INIT_PROGRESS_DURATION_MS  3000  /* 3-second fake init animation */

/* ── SOH Sliding Window ── */
#define SOH_WINDOW_SIZE       1200   /* samples: 10 min @ 500ms = 600s/0.5s */

/* ── Convergence Detection (PINN) ── */
#define CONVERGENCE_EPSILON       0.005f  /* stddev < 0.5% → candidate */
#define CONVERGENCE_MIN_SAMPLES   60      /* at least 60 samples (30s @ 2Hz) */
#define CONVERGENCE_STABLE_CHECKS 3       /* N consecutive windows under epsilon */

/* ── Chart ── */
#define CHART_WINDOW_SECONDS  300    /* rolling 5-minute window (V/A mode) */
#define CHART_MAX_POINTS      1500   /* 300s / 0.2s per sample */

/* ── Alarm Thresholds ── */
#define ALARM_TEMP_MAX_C      60.0f  /* over-temperature */
#define ALARM_VOLTAGE_MAX_V   4.25f  /* over-voltage (LiFePO4) */
#define ALARM_CURRENT_MAX_A   100.0f /* over-current */
#define ALARM_SWELLING_THRESH 0.3f   /* cell swelling sensor (0-1) */
#define ALARM_SOH_CRITICAL    0.20f  /* critical SOH degradation */
#define ALARM_SOH_WARNING     0.40f  /* warning SOH degradation */

/* ── Battery Spec ── */
#define BATTERY_NOMINAL_V     3.2f   /* LiFePO4 nominal */
#define BATTERY_CHARGE_V      3.65f  /* LiFePO4 full charge */
#define BATTERY_CUTOFF_V      2.5f   /* LiFePO4 cutoff */
#define BATTERY_NOMINAL_MAH   2000.0f /* 18650 LiFePO4 typical */

/* ── Storage (for data providers) ── */
#define NAND_TOTAL_GB         50.0f  /* Octa-NAND W35N01JW ~1Gbit=128MB, mock 50GB */
#define STORAGE_WARN_PCT      80.0f  /* warning > 80% */
#define STORAGE_CRIT_PCT      95.0f  /* critical > 95% */

/* ── Cycle Test (for data providers) ── */
#define CYCLE_TOTAL_TARGET    2000   /* total cycles for aging test */

/* ── Alarm Popup ── */
#define ALARM_POPUP_WIDTH     420
#define ALARM_POPUP_HEIGHT    180

/* ── Theme Colors ── */
#define COLOR_BG_MAIN         "#0d1117"
#define COLOR_BG_PANEL        "#161b22"
#define COLOR_BORDER          "#30363d"
#define COLOR_TEXT            "#c9d1d9"
#define COLOR_TEXT_DIM        "#8b949e"
#define COLOR_ACCENT_CYAN     "#00d4ff"
#define COLOR_ACCENT_ORANGE   "#ff6b35"
#define COLOR_HEALTHY_GREEN   "#3fb950"
#define COLOR_WARNING_YELLOW  "#d29922"
#define COLOR_CRITICAL_RED    "#f85149"
#define COLOR_PROGRESS_BLUE   "#1f6feb"

/* ── Font ── */
#define FONT_FAMILY           "DejaVu Sans Mono"
/* CJK-capable fonts (searched in order, first match wins):
 *   - "Noto Sans CJK SC" — Google Noto, best coverage
 *   - "WenQuanYi Micro Hei" — lightweight, common on embedded
 *   - "Source Han Sans SC" — Adobe, same as Noto
 *   - "DejaVu Sans Mono" — fallback (no CJK, English only) */
#define FONT_FAMILY_CJK       "Noto Sans CJK SC"
#define FONT_SIZE_DEFAULT     12
#define FONT_SIZE_TITLE       16
#define FONT_SIZE_VALUE       22
#define FONT_SIZE_HEADER      14
#define FONT_SIZE_LARGE       32

#endif /* HMI_APPCONFIG_H */
