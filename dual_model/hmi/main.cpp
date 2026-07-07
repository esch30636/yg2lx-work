/*===========================================================================
 * main.cpp — Battery SOH Assessment HMI entry point
 *
 * Usage:
 *   battery_hmi [--demo] [--file <path>] [--spi <dev>] [--spi-mock]
 *               [--fullscreen] [--lang <zh|en>]
 *
 * Examples:
 *   battery_hmi --demo                        Demo mode with synthetic data
 *   battery_hmi --file /data/session_001.bin   Replay recorded session
 *   battery_hmi --spi /dev/spidev0.0           RA8 real-time via SPI
 *   battery_hmi --spi-mock --lang zh           SPI mock + Chinese UI
 *   battery_hmi --demo --fullscreen             Fullscreen demo
 *
 * Environment variables for Qt QPA:
 *   QT_QPA_PLATFORM=eglfs                      GPU-accelerated (Mali-G31)
 *   QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0        Framebuffer fallback
 *   QT_QPA_EGLFS_INTEGRATION=none               Disable platform integration
 *   LANG=zh_CN                                  Override UI language
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
static void crashHandler(int sig)
{
    const char *name = (sig == SIGSEGV) ? "SIGSEGV" :
                       (sig == SIGABRT) ? "SIGABRT" :
                       (sig == SIGFPE)  ? "SIGFPE"  : "SIGNAL";
    /* Write directly via write(2) — async-signal-safe */
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
                       "\n[HMI] FATAL: %s (signal %d) — process terminating\n",
                       name, sig);
    write(STDERR_FILENO, buf, len);

    /* Attempt backtrace (best-effort, not async-signal-safe but useful) */
    void *bt[32];
    int n = backtrace(bt, 32);
    backtrace_symbols_fd(bt, n, STDERR_FILENO);

    /* Re-raise with default handler to produce core dump */
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

#include "config/AppConfig.h"
#include "data/DemoDataProvider.h"
#include "data/FileDataProvider.h"
#include "data/SpiDataProvider.h"
#include "ui/MainWindow.h"

/* ── Attempt to load CJK font (one-shot, called before app.setFont) ── */
static void setupCjkFont(QApplication &app)
{
    /* Font families to try, in preference order */
    static const char *candidates[] = {
        "Noto Sans CJK SC",        /* Google Noto — best coverage */
        "Source Han Sans SC",      /* Adobe — same glyphs as Noto */
        "WenQuanYi Micro Hei",     /* Lightweight, common on embedded Linux */
        "WenQuanYi Zen Hei",       /* WQY alternative */
        "Droid Sans Fallback",     /* Android-era CJK fallback */
        "DejaVu Sans Mono",        /* Last resort — no CJK, English only */
        nullptr
    };

    /* ── Strategy A: Look for a .ttf/.otf file in common locations ── */
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

    /* ── Strategy B: Try font family names (if already installed system-wide) ── */
    QFontDatabase fdb;
    for (const char **fam = candidates; *fam != nullptr; fam++) {
        if (fdb.families().contains(QLatin1String(*fam))) {
            QFont cjkFont(*fam, FONT_SIZE_DEFAULT);
            cjkFont.setStyleHint(QFont::Monospace);
            app.setFont(cjkFont);
            printf("[HMI] CJK font active: %s\n", *fam);
            return;
        }
    }

    /* Fallback: use the system default */
    QFont fallback(FONT_FAMILY, FONT_SIZE_DEFAULT);
    fallback.setStyleHint(QFont::Monospace);
    app.setFont(fallback);
    printf("[HMI] No CJK font found — using %s (Chinese chars may render as tofu)\n",
           FONT_FAMILY);
}

/* ── Load translations from resource or filesystem ── */
static void setupTranslations(QApplication &app, const QString &lang)
{
    /* Determine locale: CLI arg > env LANG > system locale */
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

    /* If locale is already English (C/POSIX/en_US), skip translator loading */
    if (locale.language() == QLocale::C || locale.language() == QLocale::English) {
        printf("[HMI] English locale — translations not loaded\n");
        return;
    }

    /* Try loading from Qt resource first, then filesystem */
    auto *translator = new QTranslator(&app);

    /* Resource path: :/translations/battery_hmi_zh_CN.qm */
    QString resPath = QString(":/translations/battery_hmi_%1.qm").arg(locale.name());
    bool loaded = translator->load(resPath);

    if (!loaded) {
        /* Filesystem: look next to the binary */
        QString fsPath = QString("%1/../translations/battery_hmi_%2.qm")
                         .arg(QCoreApplication::applicationDirPath())
                         .arg(locale.name());
        loaded = translator->load(fsPath);
    }

    if (!loaded) {
        /* Try base name without country suffix: battery_hmi_zh.qm */
        QString shortName = locale.name().left(2);
        QString resShort = QString(":/translations/battery_hmi_%1.qm").arg(shortName);
        loaded = translator->load(resShort);
    }

    if (loaded) {
        app.installTranslator(translator);
        printf("[HMI] Translations loaded for %s\n", qPrintable(locale.name()));
    } else {
        delete translator;
        printf("[HMI] No translation found for %s — using English\n",
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
     * No longer force linuxfb — wayland-egl is the preferred backend
     * for mouse-driven interaction on the MYD-YG2LX.
     * Set QT_QPA_PLATFORM=linuxfb to override for framebuffer-only mode. */

    /* ── Application ── */
    QApplication app(argc, argv);
    app.setApplicationName("Battery HMI");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("BatteryTeam");

    /* ── Font setup (must happen before any widget creation) ── */
    setupCjkFont(app);

    /* ── Command-line parsing ── */
    QCommandLineParser parser;
    parser.setApplicationDescription("Battery SOH Assessment — Industrial HMI");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption demoOption("demo", "Use synthetic demo data");
    parser.addOption(demoOption);

    QCommandLineOption fileOption("file", "Replay binary session file", "path");
    parser.addOption(fileOption);

    QCommandLineOption spiOption("spi", "Read from RA8 via SPI device", "device");
    parser.addOption(spiOption);

    QCommandLineOption spiMockOption("spi-mock", "SPI provider in mock mode (no hardware)");
    parser.addOption(spiMockOption);

    QCommandLineOption fullscreenOption("fullscreen", "Start in fullscreen mode");
    parser.addOption(fullscreenOption);

    QCommandLineOption langOption("lang", "UI language (zh|en)", "locale");
    parser.addOption(langOption);

    parser.process(app);

    /* ── Load translations (after parsing --lang) ── */
    setupTranslations(app, parser.value(langOption));

    /* ── Data provider selection ── */
    DataProvider *provider = nullptr;

    if (parser.isSet(spiMockOption)) {
        /* SPI mock mode: uses SpiDataProvider with synthetic data */
        SpiDataProvider::Config cfg;
        SpiDataProvider *sp = new SpiDataProvider(cfg);
        provider = sp;
        printf("[HMI] SPI mock mode (RA8 simulation)\n");
    } else if (parser.isSet(spiOption)) {
        /* SPI real mode: RA8 over physical SPI bus */
        SpiDataProvider::Config cfg;
        cfg.device = parser.value(spiOption);
        SpiDataProvider *sp = new SpiDataProvider(cfg);
        provider = sp;
        printf("[HMI] SPI mode: %s (%s)\n", qPrintable(cfg.device),
               sp->isOpen() ? "connected" : "mock fallback");
    } else if (parser.isSet(fileOption)) {
        QString path = parser.value(fileOption);
        FileDataProvider *fp = new FileDataProvider(path);
        if (!fp->isValid()) {
            fprintf(stderr, "ERROR: Cannot open or parse file: %s\n",
                    qPrintable(path));
            delete fp;
            return 1;
        }
        provider = fp;
        printf("[HMI] File replay mode: %s\n", qPrintable(path));
    } else {
        /* Default: demo mode */
        DemoDataProvider *dp = new DemoDataProvider(42);
        provider = dp;
        printf("[HMI] Demo mode (synthetic data, seed=42)\n");
    }

    /* ── Create and show main window ── */
    MainWindow window(provider);
    window.setWindowTitle(QString("Battery SOH Assessment — %1").arg(provider->name()));

    if (parser.isSet(fullscreenOption) || qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        /* On embedded, go fullscreen */
        window.showFullScreen();
    } else {
        window.show();
    }

    printf("[HMI] Application started. Close window or Ctrl+C to exit.\n");

    int result = app.exec();

    delete provider;
    return result;
}
