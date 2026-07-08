/*===========================================================================
 * AppConfig.h — Central compile-time constants for Battery HMI
 *
 * v3.0 — 1920×1080 oscilloscope-style light theme
 *
 * Controls: refresh intervals, alarm thresholds, chart window, layout sizing
 *===========================================================================*/
#ifndef HMI_APPCONFIG_H
#define HMI_APPCONFIG_H

/* ── Window Geometry (1920×1080) ── */
#define WINDOW_WIDTH          1920
#define WINDOW_HEIGHT         1080
#define HEADER_HEIGHT         48
#define FOOTER_HEIGHT         0
#define STATUS_BAR_HEIGHT     28
#define CHART_AREA_HEIGHT     780     /* oscilloscope — dominant element */
#define HEALTH_BAR_HEIGHT     32
#define PROGRESS_BAR_HEIGHT   28

/* ── Refresh Intervals (milliseconds) ── */
#define DATA_ACQUISITION_MS   100     /* sensor read: 10 Hz */
#define PINN_INFERENCE_MS     500     /* SOH update: 2 Hz */
#define CNN_INFERENCE_MS      2000    /* Stage+RUL update: 0.5 Hz */
#define ALARM_CHECK_MS        200     /* threshold poll: 5 Hz */
#define CLOCK_UPDATE_MS       1000    /* header clock: 1 Hz */
#define INIT_PROGRESS_MS      30      /* progress bar animation: ~30 fps */

/* ── Init Screen ── */
#define INIT_PROGRESS_DURATION_MS  3000  /* 3-second fake init animation */

/* ── SOH Sliding Window ── */
#define SOH_WINDOW_SIZE       1200   /* samples: 10 min @ 500ms = 600s/0.5s */

/* ── Convergence Detection (PINN) ── */
#define CONVERGENCE_EPSILON       0.005f  /* stddev < 0.5% → candidate */
#define CONVERGENCE_MIN_SAMPLES   60      /* at least 60 samples (30s @ 2Hz) */
#define CONVERGENCE_STABLE_CHECKS 3       /* N consecutive windows under epsilon */

/* ── Chart (Oscilloscope) ── */
#define CHART_WINDOW_SECONDS  60     /* rolling 60-second window (oscilloscope) */
#define CHART_MAX_POINTS      6000   /* 60s / 0.01s per sample */

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
#define ALARM_POPUP_WIDTH     520
#define ALARM_POPUP_HEIGHT    220

/* ═════════════════════════════════════════════════════════════════════
 * Light Theme Colors (v3.0 — 浅色界面)
 * ═════════════════════════════════════════════════════════════════════ */
#define COLOR_BG_MAIN         "#F0F0F5"   /* main background — light gray */
#define COLOR_BG_PANEL        "#FFFFFF"   /* panel/card background — white */
#define COLOR_BORDER          "#D0D0D8"   /* border — medium gray */
#define COLOR_TEXT            "#1A1A2E"   /* primary text — near-black */
#define COLOR_TEXT_DIM        "#555566"   /* secondary text — dark gray */
#define COLOR_ACCENT_CYAN     "#0066CC"   /* voltage trace — blue */
#define COLOR_ACCENT_ORANGE   "#CC3300"   /* current trace — red */
#define COLOR_HEALTHY_GREEN   "#22AA44"   /* healthy status — green */
#define COLOR_WARNING_YELLOW  "#CC8800"   /* warning — amber */
#define COLOR_CRITICAL_RED    "#DD2222"   /* critical alarm — red */
#define COLOR_PROGRESS_BLUE   "#3366CC"   /* progress bars — blue */

/* ── Chart-specific colors (oscilloscope) ── */
#define COLOR_CHART_BG        "#FAFBFC"   /* chart plot area — off-white */
#define COLOR_CHART_GRID      "#D8D8E0"   /* grid lines — light gray */
#define COLOR_CHART_AXIS      "#333344"   /* axis labels — dark */
#define COLOR_CHART_VOLTAGE   "#0055CC"   /* voltage trace — deep blue */
#define COLOR_CHART_CURRENT   "#DD4400"   /* current trace — red-orange */

/* ── Font (light theme — dark text on light bg) ── */
#define FONT_FAMILY           "Noto Sans CJK SC"
/* CJK-capable fonts (searched in order, first match wins):
 *   - "Noto Sans CJK SC" — Google Noto, best coverage
 *   - "Source Han Sans SC" — Adobe, same as Noto
 *   - "WenQuanYi Micro Hei" — lightweight, common on embedded
 *   - "WenQuanYi Zen Hei"  — WQY alternative
 *   - "DejaVu Sans Mono"   — fallback (no CJK, English only) */
#define FONT_FAMILY_CJK       "Noto Sans CJK SC"
#define FONT_SIZE_DEFAULT     14
#define FONT_SIZE_TITLE       20
#define FONT_SIZE_VALUE       28
#define FONT_SIZE_HEADER      16
#define FONT_SIZE_LARGE       42
#define FONT_SIZE_CHART_AXIS  12
#define FONT_SIZE_CHART_TITLE 16

#endif /* HMI_APPCONFIG_H */
