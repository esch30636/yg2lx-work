/*===========================================================================
 * main.cpp — Battery Monitoring HMI entry point (v3.0)
 *
 * v3.0: 1920×1080 light theme oscilloscope display.
 *       Voltage (blue) + Current (red) vs Time — primary chart.
 *       Dark Chinese text on light background.
 *
 * Usage:
 *   battery_hmi [--demo] [--file <path>] [--spi <dev>] [--spi-mock]
 *               [--windowed] [--lang <zh|en>]
 *
 * Examples:
 *   battery_hmi --demo                   Demo mode (default, fullscreen 1080p)
 *   battery_hmi --file /data/session.bin Replay recorded session
 *   battery_hmi --spi /dev/spidev0.0     RA8 real-time via SPI
 *   battery_hmi --windowed               Windowed mode (1024×600 min)
 *   battery_hmi --demo --lang zh          Chinese UI
 *
 * Environment variables for Qt QPA:
 *   QT_QPA_PLATFORM=wayland-egl           Wayland EGL (MYD-YG2LX)
 *   QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0   Framebuffer fallback
 *   LANG=zh_CN                             Override UI language
 *===========================================================================*/
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include <cstdio>
#include <cstdlib>
#include <csignal>

/* ── Crash diagnostics for embedded debugging ── */
#ifndef _WIN32
#include <execinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
static void crashHandler(int sig)
{
    const char *name = (sig == SIGSEGV) ? "SIGSEGV" :
                       (sig == SIGABRT) ? "SIGABRT" :
                       (sig == SIGFPE)  ? "SIGFPE"  : "SIGNAL";
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
                       "\n[HMI] FATAL: %s (signal %d) — process terminating\n",
                       name, sig);
    write(STDERR_FILENO, buf, len);

    /* Write backtrace to persistent log file on board for post-mortem analysis */
    int fd = open("/tmp/battery_hmi_crash.log",
                  O_WRONLY | O_CREAT | O_APPEND | O_SYNC, 0644);
    if (fd >= 0) {
        /* Timestamp marker */
        char marker[64];
        int ml = snprintf(marker, sizeof(marker),
                          "=== CRASH %s (sig=%d) ===\n", name, sig);
        write(fd, marker, ml);

        /* Backtrace to file */
        void *bt[32];
        int n = backtrace(bt, 32);
        backtrace_symbols_fd(bt, n, fd);
        write(fd, "\n", 1);
        close(fd);
    }

    /* Also write backtrace to stderr (may be visible on serial console) */
    void *bt2[32];
    int n2 = backtrace(bt2, 32);
    backtrace_symbols_fd(bt2, n2, STDERR_FILENO);

    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

#include "config/AppConfig.h"
#include "data/DemoDataProvider.h"
#include "data/FileDataProvider.h"
#include "data/SpiDataProvider.h"
#include "data/UartDataProvider.h"
#include "ui/MainWindow.h"

/* ── Attempt to load CJK font for light theme (dark text on light bg) ── */
static void setupCjkFont(QApplication &app)
{
    /* Font families to try, in preference order.
     * v3.0: Regular weight preferred (dark text on light background). */
    static const char *candidates[] = {
        "Noto Sans CJK SC",        /* Google Noto — best coverage */
        "Source Han Sans SC",      /* Adobe — same glyphs as Noto */
        "WenQuanYi Micro Hei",     /* Lightweight, common on embedded Linux */
        "WenQuanYi Zen Hei",       /* WQY alternative */
        "Droid Sans Fallback",     /* Android-era CJK fallback */
        "DejaVu Sans Mono",        /* Last resort — no CJK, English only */
        nullptr
    };

    /* ── Strategy A: Look for .ttf/.otf files in common locations ── */
    static const char *fontPaths[] = {
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        nullptr
    };

    for (const char **fp = fontPaths; *fp != nullptr; fp++) {
        if (QFileInfo::exists(*fp)) {
            int id = QFontDatabase::addApplicationFont(*fp);
            if (id >= 0) {
                printf("[HMI] Loaded font: %s (families: %s)\n",
                       *fp,
                       qPrintable(QFontDatabase::applicationFontFamilies(id).join(", ")));
            }
        }
    }

    /* ── Strategy B: Try font family names ── */
    QFontDatabase fdb;
    for (const char **fam = candidates; *fam != nullptr; fam++) {
        if (fdb.families().contains(QLatin1String(*fam))) {
            QFont cjkFont(*fam, FONT_SIZE_DEFAULT);
            cjkFont.setStyleHint(QFont::SansSerif);
            app.setFont(cjkFont);
            printf("[HMI] CJK font active: %s (size %d)\n", *fam, FONT_SIZE_DEFAULT);
            return;
        }
    }

    /* Fallback: use the system default */
    QFont fallback(FONT_FAMILY, FONT_SIZE_DEFAULT);
    fallback.setStyleHint(QFont::SansSerif);
    app.setFont(fallback);
    printf("[HMI] No CJK font found — using %s (Chinese chars may render as tofu)\n",
           FONT_FAMILY);
}

/* ── Load translations from resource or filesystem ── */
static void setupTranslations(QApplication &app, const QString &lang)
{
    QLocale locale;
    if (!lang.isEmpty()) {
        locale = QLocale(lang);
    } else if (!qEnvironmentVariableIsEmpty("LANG")) {
        locale = QLocale(qgetenv("LANG"));
    } else {
        locale = QLocale::system();
    }

    printf("[HMI] Locale: %s (%s)\n",
           qPrintable(locale.name()),
           qPrintable(locale.nativeLanguageName()));

    /* If locale is English, skip translator loading (tr() returns source text) */
    if (locale.language() == QLocale::C || locale.language() == QLocale::English) {
        printf("[HMI] English locale — using source strings (Chinese)\n");
        return;
    }

    auto *translator = new QTranslator(&app);

    QString resPath = QString(":/translations/battery_hmi_%1.qm").arg(locale.name());
    bool loaded = translator->load(resPath);

    if (!loaded) {
        QString fsPath = QString("%1/../translations/battery_hmi_%2.qm")
                         .arg(QCoreApplication::applicationDirPath())
                         .arg(locale.name());
        loaded = translator->load(fsPath);
    }

    if (!loaded) {
        QString shortName = locale.name().left(2);
        QString resShort = QString(":/translations/battery_hmi_%1.qm").arg(shortName);
        loaded = translator->load(resShort);
    }

    if (loaded) {
        app.installTranslator(translator);
        printf("[HMI] Translations loaded for %s\n", qPrintable(locale.name()));
    } else {
        delete translator;
        printf("[HMI] No translation found for %s — using source strings\n",
               qPrintable(locale.name()));
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    /* ── Crash diagnostics (embedded) ── */
#ifndef _WIN32
    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);
    signal(SIGFPE,  crashHandler);
#endif

    /* ── Qt Platform setup ──
     * wayland-egl is the preferred backend for MYD-YG2LX.
     * Set QT_QPA_PLATFORM=linuxfb to override for framebuffer-only mode. */

    /* ── Application ── */
    QApplication app(argc, argv);
    app.setApplicationName("电池监测 HMI");
    app.setApplicationVersion("3.0.0");
    app.setOrganizationName("BatteryTeam");

    /* ── Font setup (must happen before any widget creation) ── */
    setupCjkFont(app);

    /* ── Command-line parsing ── */
    QCommandLineParser parser;
    parser.setApplicationDescription("电池监测 — 示波器 HMI (1920×1080 浅色界面)");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption demoOption("demo", "使用合成演示数据");
    parser.addOption(demoOption);

    QCommandLineOption fileOption("file", "回放二进制数据文件", "path");
    parser.addOption(fileOption);

    QCommandLineOption spiOption("spi", "通过 SPI 读取 RA8 实时数据", "device");
    parser.addOption(spiOption);

    QCommandLineOption spiMockOption("spi-mock", "SPI 模拟模式 (无硬件)");
    parser.addOption(spiMockOption);

    QCommandLineOption uartOption("uart", "通过 UART 串口读取 RA8 实时数据", "device");
    parser.addOption(uartOption);

    QCommandLineOption windowedOption("windowed", "窗口模式 (非全屏)");
    parser.addOption(windowedOption);

    QCommandLineOption langOption("lang", "界面语言 (zh|en)", "locale");
    parser.addOption(langOption);

    parser.process(app);

    /* ── Load translations (after parsing --lang) ── */
    setupTranslations(app, parser.value(langOption));

    /* ── Data provider selection ── */
    DataProvider *provider = nullptr;

    if (parser.isSet(spiMockOption)) {
        SpiDataProvider::Config cfg;
        SpiDataProvider *sp = new SpiDataProvider(cfg);
        provider = sp;
        printf("[HMI] SPI 模拟模式 (RA8 仿真)\n");
    } else if (parser.isSet(spiOption)) {
        SpiDataProvider::Config cfg;
        cfg.device = parser.value(spiOption);
        SpiDataProvider *sp = new SpiDataProvider(cfg);
        provider = sp;
        printf("[HMI] SPI 模式: %s (%s)\n", qPrintable(cfg.device),
               sp->isOpen() ? "已连接" : "模拟回退");
    } else if (parser.isSet(uartOption)) {
        UartDataProvider::Config cfg;
        cfg.device = parser.value(uartOption);
        UartDataProvider *up = new UartDataProvider(cfg);
        provider = up;
        printf("[HMI] UART 模式: %s (%s)\n", qPrintable(cfg.device),
               up->isOpen() ? "已连接" : "未连接");
    } else if (parser.isSet(fileOption)) {
        QString path = parser.value(fileOption);
        FileDataProvider *fp = new FileDataProvider(path);
        if (!fp->isValid()) {
            fprintf(stderr, "ERROR: 无法打开文件: %s\n",
                    qPrintable(path));
            delete fp;
            return 1;
        }
        provider = fp;
        printf("[HMI] 文件回放模式: %s\n", qPrintable(path));
    } else {
        /* Default: demo mode */
        DemoDataProvider *dp = new DemoDataProvider(42);
        provider = dp;
        printf("[HMI] 演示模式 (合成数据, seed=42)\n");
    }

    /* ── Create and show main window ── */
    printf("[HMI] 创建主窗口...\n"); fflush(stdout);
    MainWindow window(provider);
    printf("[HMI] 主窗口构造完成\n"); fflush(stdout);
    window.setWindowTitle(QString("电池监测 HMI v3.0 — %1").arg(provider->name()));
    printf("[HMI] 标题已设置\n"); fflush(stdout);

    /* v3.1: On Wayland, showFullScreen() in the constructor triggers a
     * Wayland protocol error (xdg-shell surface not yet configured when
     * setVisible is called). Instead, set the window state BEFORE show()
     * so the fullscreen hint is applied during initial surface creation.
     * This avoids the QWaylandDisplay::checkError() → abort() path. */
    if (!parser.isSet(windowedOption)) {
        window.setWindowState(Qt::WindowFullScreen);
        printf("[HMI] 全屏模式 (1920×1080)\n"); fflush(stdout);
    } else {
        printf("[HMI] 窗口模式 (minimum 1024×600)\n"); fflush(stdout);
    }
    printf("[HMI] 即将显示窗口...\n"); fflush(stdout);
    window.show();
    printf("[HMI] 窗口已显示\n"); fflush(stdout);

    printf("[HMI] 应用已启动。关闭窗口或 Ctrl+C 退出。\n");

    int result = app.exec();

    delete provider;
    return result;
}
