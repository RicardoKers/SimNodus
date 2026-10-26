// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "analyzer_window.hpp"
#include "app_settings.hpp"
#include "engine_process.hpp"
#include "main_window.hpp"
#include "theme.hpp"

#include "emu/elf.hpp"
#include "emu/stm32f103.hpp"
#include "io/project_file.hpp"
#include "library/blocks.hpp"
#include "library/component_library.hpp"
#include "sim/remote_session.hpp"
#include "sim/engine_check.hpp"
#include "simnodus/version.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QSplitter>
#include <QTimer>
#include <QTranslator>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <vector>

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

// The interface language (UI-16): the setting "language" ("en", "pt_BR"), or
// the system's language when it is empty.
void install_translations(QApplication& application, const QString& language)
{
    if (language == QStringLiteral("en")) {
        return;
    }
    const QLocale locale = language.isEmpty() ? QLocale() : QLocale(language);
    QLocale::setDefault(locale);
    const QString app_dir = QCoreApplication::applicationDirPath();
    const QStringList folders{app_dir + QStringLiteral("/translations"),
        app_dir + QStringLiteral("/../share/simnodus/translations")};
    auto* qt = new QTranslator(&application);
    bool qt_loaded = qt->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
        QLibraryInfo::path(QLibraryInfo::TranslationsPath));
    for (const auto& folder : folders) {
        qt_loaded = qt_loaded || qt->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"), folder);
    }
    if (qt_loaded) {
        application.installTranslator(qt);
    }
    auto* own = new QTranslator(&application);
    for (const auto& folder : folders) {
        if (own->load(locale, QStringLiteral("simnodus"), QStringLiteral("_"), folder)) {
            application.installTranslator(own);
            application.setProperty("simnodus_language", own->language()); // Help > User Guide in this language
            break;
        }
    }
}

// Runs example 01 for 0.1 s of simulated time in the engine process (SN-151),
// as the editor does.
int check_engine_process(const std::filesystem::path& app_dir)
{
    simnodus::sim::protocol::EngineSetup setup;
    setup.application_dir = app_dir;
    simnodus::library::ComponentLibrary library;
    for (const auto& folder : simnodus::library::library_candidates(app_dir)) {
        std::error_code ec;
        if (std::filesystem::is_directory(folder, ec)) {
            library.load_directory(folder);
            setup.library_dir = folder;
            break;
        }
    }
    std::optional<simnodus::model::Schematic> circuit;
    std::vector<std::filesystem::path> folders{app_dir / "examples", app_dir / ".." / "share" / "simnodus" / "examples"};
#ifdef SIMNODUS_SOURCE_EXAMPLES_DIR
    folders.emplace_back(SIMNODUS_SOURCE_EXAMPLES_DIR); // a build tree
#endif
    for (const auto& examples : folders) {
        if (auto loaded = simnodus::io::load_project(examples / "01_LED_and_push_button.snx"); loaded.schematic) {
            circuit = std::move(loaded.schematic);
            break;
        }
    }
    if (!circuit || library.empty()) {
        std::printf("FAIL: the library or examples/01_LED_and_push_button.snx was not found next to the application\n");
        return 1;
    }
    simnodus::ui::EngineProcess engine;
    simnodus::sim::RemoteSession session(engine, setup);
    simnodus::sim::RunRequest request;
    request.schematic = *circuit;
    const auto library_with_blocks = simnodus::library::with_blocks(library, request.schematic);
    request.library = &library_with_blocks;
    std::string error;
    const auto begin = std::chrono::steady_clock::now();
    if (!session.start(request, error)) {
        std::printf("FAIL: engine process %s: %s\n", simnodus::ui::EngineProcess::program().toLocal8Bit().constData(),
            error.c_str());
        return 1;
    }
    while (session.active() && session.status().sim_time < 0.1
        && std::chrono::steady_clock::now() - begin < std::chrono::seconds(30)) {
        engine.pump(50);
    }
    const auto status = session.status();
    session.stop();
    if (status.sim_time < 0.1) {
        std::printf("FAIL: the engine process reached %.3f s of simulated time: %s\n", status.sim_time,
            status.message.c_str());
        return 1;
    }
    std::printf("PASS: engine process ran example 01 for %.2f s of simulated time\n", status.sim_time);
    return 0;
}

int check_engine()
{
    const std::filesystem::path app_dir = QCoreApplication::applicationDirPath().toStdWString();
    const auto result = simnodus::sim::check_analog_engine(app_dir);
    for (const auto& line : result.messages) {
        std::printf("%s\n", line.c_str());
    }
    if (!result.ok) {
        std::printf("FAIL: analog engine not available\n");
        return 1;
    }
    std::printf("PASS: ngspice-%d loaded from %s; divider V(out) = %.6f V\n", result.version.value_or(0),
        result.library.string().c_str(), result.divider_volts);
    return check_engine_process(app_dir);
}

// Runs the blink example's firmware for 1.1 s of virtual time in the
// microcontroller emulator; PC13 must toggle every 500 ms.
int check_mcu_engine()
{
    const std::filesystem::path app_dir = QCoreApplication::applicationDirPath().toStdWString();
    std::filesystem::path firmware;
    for (const auto& examples : {app_dir / "examples", app_dir / ".." / "share" / "simnodus" / "examples"}) {
        if (std::filesystem::exists(examples / "firmware" / "blink" / "blink.elf")) {
            firmware = examples / "firmware" / "blink" / "blink.elf";
            break;
        }
    }
    if (firmware.empty()) {
        std::printf("FAIL: examples/firmware/blink/blink.elf was not found next to the application\n");
        return 1;
    }
    std::string error;
    const auto image = simnodus::emu::read_elf(firmware, error);
    simnodus::emu::Stm32F103 machine;
    if (!image || !machine.load(*image, error)) {
        std::printf("FAIL: %s\n", error.c_str());
        return 1;
    }
    const auto begin = std::chrono::steady_clock::now();
    machine.run_until_ns(1100000000);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    int toggles = 0;
    for (const auto& e : machine.take_pin_events()) {
        toggles += e.port == 2 && e.pin == 13 ? 1 : 0;
    }
    if (toggles < 2) {
        std::printf("FAIL: the blink firmware toggled PC13 %d time(s) in 1.1 s of virtual time\n", toggles);
        return 1;
    }
    std::printf("PASS: microcontroller emulator ran blink.elf for 1.1 s of virtual time in %.2f s; PC13 toggled %d times\n",
        seconds, toggles);
    return 0;
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
    expect(!QApplication::windowIcon().availableSizes().isEmpty(), "application icon");
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
    {
        QIcon icon;
        for (const int size : {16, 24, 32, 48, 64, 128, 256}) {
            icon.addFile(QStringLiteral(":/icons/simnodus_%1.png").arg(size), QSize(size, size));
        }
        QApplication::setWindowIcon(icon);
    }
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
        QCoreApplication::translate("main", "Run the blink example's firmware in the microcontroller emulator and exit."));
    const QCommandLineOption screenshot_option(QStringLiteral("screenshot"),
        QCoreApplication::translate("main", "Save images of the windows to this PNG file and exit."),
        QStringLiteral("file"));
    const QCommandLineOption delay_option(QStringLiteral("screenshot-delay"),
        QCoreApplication::translate("main", "Milliseconds before the screenshot (default 300, 2500 with --run)."),
        QStringLiteral("ms"));
    const QCommandLineOption run_option(QStringLiteral("run"),
        QCoreApplication::translate("main", "Start the simulation after opening the project."));
    const QCommandLineOption control_option(QStringLiteral("control"),
        QCoreApplication::translate("main", "Allow external control by AI agents and scripts (simnodus-cli connect)."));
    parser.addOptions({settings_option, smoke_option, engine_option, mcu_option, screenshot_option, delay_option,
        run_option, control_option});
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

    const auto settings = simnodus::ui::open_settings(parser.value(settings_option));
    simnodus::ui::theme::apply(application,
        simnodus::ui::theme::scheme_from(settings->value(QStringLiteral("theme")).toString()));
    install_translations(application, settings->value(QStringLiteral("language")).toString());
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
    if (parser.isSet(control_option)) {
        QString error;
        if (!window.set_external_control(true, &error)) {
            attach_console();
            std::fprintf(stderr, "%s\n", qPrintable(error));
        }
    }
    if (parser.isSet(screenshot_option)) {
        attach_console();
        const int delay = parser.isSet(delay_option) ? parser.value(delay_option).toInt()
            : parser.isSet(run_option)                ? 2500
                                                      : 300;
        return screenshot(window, parser.value(screenshot_option), delay);
    }
    window.show();
    window.show_restored_windows();
    if (parser.positionalArguments().isEmpty()) {
        QTimer::singleShot(0, &window, [&window] { window.check_recovery(); });
    }
    return QApplication::exec();
}
