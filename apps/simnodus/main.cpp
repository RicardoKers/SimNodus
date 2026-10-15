// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "analyzer_window.hpp"
#include "app_settings.hpp"
#include "main_window.hpp"
#include "theme.hpp"

#include "mcu/mcu_check.hpp"
#include "sim/engine_check.hpp"
#include "simnodus/version.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QSplitter>
#include <QTimer>
#include <QTranslator>

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

// A GUI-subsystem executable has no console; reuse the caller's console for
// command-line modes when output is not already redirected.
void attach_console()
{
#ifdef _WIN32
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN && AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* ignored = nullptr;
        freopen_s(&ignored, "CONOUT$", "w", stdout);
        freopen_s(&ignored, "CONOUT$", "w", stderr);
    }
#endif
}

void install_translations(QApplication& application)
{
    const QLocale locale;
    auto* qt = new QTranslator(&application);
    if (qt->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
            QLibraryInfo::path(QLibraryInfo::TranslationsPath))
        || qt->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/translations"))) {
        application.installTranslator(qt);
    }
    auto* own = new QTranslator(&application);
    if (own->load(locale, QStringLiteral("simnodus"), QStringLiteral("_"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/translations"))) {
        application.installTranslator(own);
    }
}

int check_engine()
{
    const std::filesystem::path app_dir = QCoreApplication::applicationDirPath().toStdWString();
    const auto result = simnodus::sim::check_analog_engine(app_dir);
    for (const auto& line : result.messages) {
        std::printf("%s\n", line.c_str());
    }
    if (result.ok) {
        std::printf("PASS: ngspice-%d loaded from %s; divider V(out) = %.6f V\n", result.version.value_or(0),
            result.library.string().c_str(), result.divider_volts);
        return 0;
    }
    std::printf("FAIL: analog engine not available\n");
    return 1;
}

int check_mcu_engine()
{
    const std::filesystem::path app_dir = QCoreApplication::applicationDirPath().toStdWString();
    const auto result = simnodus::mcu::check_mcu_engine(app_dir);
    for (const auto& line : result.messages) {
        std::printf("%s\n", line.c_str());
    }
    if (result.ok) {
        std::printf("PASS: Renode from %s started in %.1f s, ran 1 ms of virtual time, exited cleanly\n",
            result.renode.string().c_str(), result.startup_seconds);
        return 0;
    }
    std::printf("FAIL: microcontroller engine not available\n");
    return 1;
}

// Opens the windows, exercises the panel and analyzer actions and exits.
// Used to check packaged builds on a real desktop or with -platform offscreen.
int smoke_test(simnodus::ui::MainWindow& window)
{
    using simnodus::ui::Panel;
    int failures = 0;
    auto expect = [&](bool condition, const char* what) {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", what);
        failures += condition ? 0 : 1;
    };
    window.show();
    QCoreApplication::processEvents();
    expect(window.columns()->count() == 3, "three columns");
    window.set_panel_visible(Panel::Components, false);
    expect(!window.panel_visible(Panel::Components), "components panel hides");
    window.set_panel_visible(Panel::Components, true);
    expect(window.panel_visible(Panel::Components), "components panel returns");
    window.show_analyzer();
    QCoreApplication::processEvents();
    expect(window.analyzer()->isVisible(), "analyzer window opens");
    window.analyzer()->close();
    QCoreApplication::processEvents();
    expect(!window.analyzer()->isVisible(), "analyzer window hides on close");
    window.close();
    return failures == 0 ? 0 : 1;
}

// Saves images of the main window and the analyzer (documentation, reviews).
int screenshot(simnodus::ui::MainWindow& window, const QString& file, int delay_ms)
{
    window.resize(1280, 800);
    window.show();
    window.show_analyzer();
    window.analyzer()->resize(960, 560);
    QTimer::singleShot(delay_ms, &window, [&window, file] {
        const QFileInfo info(file);
        const QString analyzer_file = info.path() + QStringLiteral("/") + info.completeBaseName()
            + QStringLiteral("-analyzer.") + info.suffix();
        const bool ok = window.grab().save(file) && window.analyzer()->grab().save(analyzer_file);
        std::printf("%s %s and %s\n", ok ? "Saved" : "FAILED to save", qPrintable(file), qPrintable(analyzer_file));
        QCoreApplication::exit(ok ? 0 : 1);
    });
    return QApplication::exec();
}

} // namespace

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SimNodus"));
    QCoreApplication::setApplicationName(QStringLiteral("SimNodus"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SIMNODUS_VERSION_FULL));
    install_translations(application);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Interactive circuit and STM32 simulator"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption settings_option(QStringLiteral("settings"),
        QCoreApplication::translate("main", "Use this settings file instead of the user's settings."),
        QStringLiteral("file"));
    const QCommandLineOption smoke_option(QStringLiteral("smoke-test"),
        QCoreApplication::translate("main", "Open the windows, run built-in checks and exit."));
    const QCommandLineOption engine_option(QStringLiteral("check-engine"),
        QCoreApplication::translate("main", "Load the analog engine, solve a reference circuit and exit."));
    const QCommandLineOption mcu_option(QStringLiteral("check-mcu-engine"),
        QCoreApplication::translate("main", "Start the microcontroller engine, run 1 ms and exit."));
    const QCommandLineOption screenshot_option(QStringLiteral("screenshot"),
        QCoreApplication::translate("main", "Save images of the windows to this PNG file and exit."),
        QStringLiteral("file"));
    const QCommandLineOption run_option(QStringLiteral("run"),
        QCoreApplication::translate("main", "Start the simulation after opening the project."));
    parser.addOptions({settings_option, smoke_option, engine_option, mcu_option, screenshot_option, run_option});
    parser.addPositionalArgument(QStringLiteral("project"),
        QCoreApplication::translate("main", "Project file (.snx) to open."), QStringLiteral("[project]"));
    parser.process(application);

    if (parser.isSet(engine_option)) {
        attach_console();
        return check_engine();
    }
    if (parser.isSet(mcu_option)) {
        attach_console();
        return check_mcu_engine();
    }

    simnodus::ui::theme::apply(application);
    const auto settings = simnodus::ui::open_settings(parser.value(settings_option));
    simnodus::ui::MainWindow window(*settings);
    if (parser.isSet(smoke_option)) {
        attach_console();
        return smoke_test(window);
    }
    if (!parser.positionalArguments().isEmpty()) {
        window.open_file(parser.positionalArguments().constFirst());
    }
    if (parser.isSet(run_option)) {
        QTimer::singleShot(0, &window, [&window] { window.run(); });
    }
    if (parser.isSet(screenshot_option)) {
        attach_console();
        return screenshot(window, parser.value(screenshot_option), parser.isSet(run_option) ? 2500 : 300);
    }
    window.show();
    window.show_restored_windows();
    if (parser.positionalArguments().isEmpty()) {
        QTimer::singleShot(0, &window, [&window] { window.check_recovery(); });
    }
    return QApplication::exec();
}
