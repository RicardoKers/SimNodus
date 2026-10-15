// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// End-to-end tests of the Circuit Editor and the run controls (SN-112..SN-122),
// driven through the real main window, offscreen.
#include "analyzer_window.hpp"
#include "app_settings.hpp"
#include "document.hpp"
#include "main_window.hpp"
#include "messages_panel.hpp"
#include "properties_panel.hpp"
#include "run_controller.hpp"
#include "schematic_scene.hpp"
#include "schematic_view.hpp"
#include "theme.hpp"

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "editing/operations.hpp"
#include "io/project_file.hpp"
#include "netlist/connectivity.hpp"
#include "sim/engine_host.hpp"
#endif

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QPointer>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

#include <cmath>
#include <memory>

using namespace simnodus;
using namespace simnodus::ui;

class EditorTests : public QObject {
    Q_OBJECT

private:
    QTemporaryDir dir_;
    std::unique_ptr<QSettings> settings_;
    MainWindow* window_ = nullptr;
    int counter_ = 0;

    Document& doc() { return *window_->document(); }
    SchematicView* view() { return window_->schematic(); }

    QPoint at(model::Point grid) const { return window_->schematic()->mapFromScene(to_scene(grid)); }

    // QTest::mouseMove does not deliver move events offscreen; send them.
    void move_to(model::Point grid, Qt::MouseButtons buttons)
    {
        const QPointF local = at(grid);
        QMouseEvent event(QEvent::MouseMove, local, view()->viewport()->mapToGlobal(local), Qt::NoButton, buttons,
            Qt::NoModifier);
        QApplication::sendEvent(view()->viewport(), &event);
    }

    model::Id place(const char* type, model::Point position, model::Orientation o = {})
    {
        model::Id id = model::no_id;
        doc().apply(editing::add_component(doc().id_source(), *window_->library().find(type), position, o, &id));
        return id;
    }

    void label(model::Id component, const char* pin, const char* name)
    {
        const auto* c = doc().schematic().component(component);
        const auto* d = window_->library().find(c->type);
        doc().apply(editing::add_label(doc().id_source(), library::pin_position(*d, *d->pin(pin), *c), name));
    }

    static bool engine_available()
    {
#ifdef SIMNODUS_TEST_NGSPICE_LIBRARY
        return true;
#else
        return false;
#endif
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        theme::apply(*qApp);
#ifdef SIMNODUS_TEST_NGSPICE_LIBRARY
        sim::EngineHost::instance().set_location({SIMNODUS_TEST_NGSPICE_LIBRARY, SIMNODUS_TEST_NGSPICE_SCRIPTS});
#endif
    }

    void init()
    {
        settings_ = open_settings(dir_.filePath(QStringLiteral("editor-%1.ini").arg(++counter_)));
        window_ = new MainWindow(*settings_);
        window_->set_confirm_discard(true);
        window_->resize(1200, 800);
        window_->show();
        QVERIFY(QTest::qWaitForWindowExposed(window_));
        QVERIFY(!window_->library().empty());
    }

    void cleanup()
    {
        delete window_;
        window_ = nullptr;
    }

    void placing_by_clicks_numbers_designators_and_undoes()
    {
        view()->begin_place(window_->library().find("resistor"));
        QCOMPARE(view()->tool(), Tool::Place);
        move_to({10, 10}, Qt::NoButton);
        QTest::mouseClick(view()->viewport(), Qt::LeftButton, {}, at({10, 10}));
        move_to({20, 10}, Qt::NoButton);
        QTest::mouseClick(view()->viewport(), Qt::LeftButton, {}, at({20, 10}));
        QTest::keyClick(view(), Qt::Key_Escape);
        QCOMPARE(view()->tool(), Tool::Select);
        const auto& s = doc().schematic();
        QCOMPARE(s.components().size(), std::size_t{2});
        QCOMPARE(s.components()[0].designator, std::string("R1"));
        QCOMPARE(s.components()[1].designator, std::string("R2"));
        QCOMPARE(s.components()[0].position, (model::Point{10, 10}));
        doc().undo_stack()->undo();
        QCOMPARE(doc().schematic().components().size(), std::size_t{1});
        doc().undo_stack()->redo();
        QCOMPARE(doc().schematic().components().size(), std::size_t{2});
        QVERIFY(window_->isWindowModified());
    }

    void wire_tool_connects_two_pins()
    {
        const model::Id a = place("resistor", {0, 0});
        const model::Id b = place("resistor", {10, 0});
        view()->set_tool(Tool::Wire);
        QTest::mouseClick(view()->viewport(), Qt::LeftButton, {}, at({0, 3}));
        move_to({10, 6}, Qt::NoButton);
        QTest::mouseClick(view()->viewport(), Qt::LeftButton, {}, at({10, 3})); // ends on a pin: finishes
        QCOMPARE(doc().schematic().wires().size(), std::size_t{1});
        const auto conn = netlist::build_connectivity(doc().schematic(), window_->library());
        QCOMPARE(conn.net_of_pin({a, "2"}), conn.net_of_pin({b, "2"}));
        QVERIFY(editing::is_orthogonal(doc().schematic().wires()[0].points));
    }

    void dragging_a_component_keeps_its_wire_attached()
    {
        const model::Id r = place("resistor", {0, 0});
        doc().apply(editing::add_wire(doc().id_source(), {{0, 3}, {0, 10}, {8, 10}}));
        QTest::mousePress(view()->viewport(), Qt::LeftButton, {}, at({0, 0}));
        move_to({3, -1}, Qt::LeftButton);
        move_to({5, -2}, Qt::LeftButton);
        QTest::mouseRelease(view()->viewport(), Qt::LeftButton, {}, at({5, -2}));
        const auto* c = doc().schematic().component(r);
        QCOMPARE(c->position, (model::Point{5, -2}));
        const auto& w = doc().schematic().wires()[0].points;
        QCOMPARE(w.front(), (model::Point{5, 1})); // pin 2 moved with the component
        QCOMPARE(w.back(), (model::Point{8, 10}));
        QVERIFY(editing::is_orthogonal(w));
        doc().undo_stack()->undo(); // one undo step for the whole drag
        QCOMPARE(doc().schematic().component(r)->position, (model::Point{0, 0}));
    }

    void properties_panel_edits_values_with_validation()
    {
        const model::Id r = place("resistor", {0, 0});
        window_->scene()->select_ids({r});
        auto* edit = window_->properties()->findChild<QLineEdit*>(QStringLiteral("parameter_R"));
        QVERIFY(edit);
        edit->setFocus();
        edit->selectAll();
        QTest::keyClicks(edit, QStringLiteral("4k7"));
        QTest::keyClick(edit, Qt::Key_Return);
        QCOMPARE(doc().schematic().component(r)->parameters.at("R"), std::string("4k7"));
        edit = window_->properties()->findChild<QLineEdit*>(QStringLiteral("parameter_R"));
        edit->setFocus();
        edit->selectAll();
        QTest::keyClicks(edit, QStringLiteral("abc"));
        QTest::keyClick(edit, Qt::Key_Return);
        QCOMPARE(doc().schematic().component(r)->parameters.at("R"), std::string("4k7")); // invalid: not applied
        QVERIFY(!edit->toolTip().isEmpty());
    }

    void leaving_a_value_field_by_clicking_the_schematic_applies_it()
    {
        // Owner report 2026-10-14: the program closed while changing a resistor
        // value. Leaving the field applies the value; that used to rebuild the
        // form and delete the field inside its own focus-out handler.
        const model::Id r1 = place("resistor", {0, 0});
        const model::Id r2 = place("resistor", {20, 0});
        window_->scene()->select_ids({r1});
        QPointer<QLineEdit> edit = window_->properties()->findChild<QLineEdit*>(QStringLiteral("parameter_R"));
        QVERIFY(edit);
        edit->selectAll();
        QTest::keyClicks(edit, QStringLiteral("2k2"));
        view()->setFocus(); // focus is elsewhere, as after a click on the schematic
        QFocusEvent focus_out(QEvent::FocusOut, Qt::MouseFocusReason);
        QApplication::sendEvent(edit, &focus_out);
        QCOMPARE(doc().schematic().component(r1)->parameters.at("R"), std::string("2k2"));
        QTest::mouseClick(view()->viewport(), Qt::LeftButton, {}, at({20, 0}));
        QCoreApplication::processEvents();
        QCOMPARE(window_->scene()->selected_ids(), (std::set<model::Id>{r2}));
        QVERIFY(edit.isNull() || edit->parent() == nullptr); // old field detached, deleted later
        auto* fresh = window_->properties()->findChild<QLineEdit*>(QStringLiteral("parameter_R"));
        QVERIFY(fresh);
        QCOMPARE(fresh->text(), QStringLiteral("1k")); // r2's value
    }

    void choosing_a_value_from_a_list_applies_it()
    {
        const model::Id led = place("led", {0, 0});
        window_->scene()->select_ids({led});
        auto* box = window_->properties()->findChild<QComboBox*>(QStringLiteral("parameter_color"));
        QVERIFY(box);
        box->setCurrentIndex(box->findData(QStringLiteral("blue")));
        emit box->activated(box->currentIndex());
        QCoreApplication::processEvents();
        QCOMPARE(doc().schematic().component(led)->parameters.at("color"), std::string("blue"));
        box = window_->properties()->findChild<QComboBox*>(QStringLiteral("parameter_color"));
        QVERIFY(box);
        QCOMPARE(box->currentData().toString(), QStringLiteral("blue"));
    }

    void copy_paste_rotate_and_delete()
    {
        const model::Id r = place("capacitor", {0, 0});
        window_->scene()->select_ids({r});
        view()->rotate();
        QCOMPARE(doc().schematic().component(r)->orientation.rotation, 90);
        view()->copy_selection();
        view()->paste();
        QCOMPARE(doc().schematic().components().size(), std::size_t{2});
        QCOMPARE(doc().schematic().components()[1].designator, std::string("C2"));
        window_->scene()->select_ids({r});
        view()->delete_selection();
        QCOMPARE(doc().schematic().components().size(), std::size_t{1});
    }

    void save_and_open_round_trip()
    {
        const model::Id r = place("resistor", {0, 0});
        label(r, "1", "IN");
        const QString path = dir_.filePath(QStringLiteral("round.snx"));
        QVERIFY(window_->save_to(path));
        QVERIFY(!window_->isWindowModified());
        QVERIFY(window_->windowTitle().startsWith(QStringLiteral("round.snx")));
        const model::Schematic saved = doc().schematic();
        window_->new_file();
        QVERIFY(doc().schematic().empty());
        QVERIFY(window_->open_file(path));
        QVERIFY(doc().schematic() == saved);
        QVERIFY(!window_->open_file(dir_.filePath(QStringLiteral("missing.snx"))));
        QVERIFY(window_->messages()->error_count() > 0);
    }

    void unsaved_work_is_recovered_after_a_crash()
    {
        place("resistor", {0, 0});
        place("capacitor", {10, 0});
        QVERIFY(window_->write_recovery());
        const QString recovery = window_->recovery_file();
        QVERIFY(QFileInfo::exists(recovery));
        const model::Schematic before = doc().schematic();
        delete window_; // like a crash: no close event, the recovery file stays
        window_ = nullptr;
        QVERIFY(QFileInfo::exists(recovery));
        window_ = new MainWindow(*settings_);
        window_->set_confirm_discard(true);
        QVERIFY(window_->check_recovery(true));
        QCOMPARE(doc().schematic().components(), before.components());
        QVERIFY(doc().file_path().isEmpty()); // restored as an untitled copy
        window_->close();                      // clean exit removes it
        QVERIFY(!QFileInfo::exists(recovery));
        QVERIFY(!window_->check_recovery(true));
    }

    void prerun_check_reports_missing_ground()
    {
        place("resistor", {0, 0});
        window_->run();
        QVERIFY(!window_->run_controller()->active());
        QVERIFY(window_->messages()->error_count() > 0);
    }

    void interactive_run_button_lights_the_led()
    {
        if (!engine_available()) {
            QSKIP("No ngspice library configured for this build.");
        }
        const model::Id v = place("vdc", {0, 0});
        label(v, "+", "VCC");
        label(v, "-", "0");
        const model::Id button = place("button_no", {20, 0});
        label(button, "1", "VCC");
        label(button, "2", "A");
        const model::Id r = place("resistor", {40, 0});
        doc().apply(editing::set_parameter(doc().schematic(), r, "R", "330"));
        label(r, "1", "A");
        label(r, "2", "K");
        const model::Id led = place("led", {60, 0});
        label(led, "A", "K");
        label(led, "K", "0");
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        QTest::qWait(300);
        QVERIFY(window_->scene()->live().led_current.count(led));
        QVERIFY(window_->scene()->live().led_current.at(led) < 1e-6); // button released: dark
        window_->run_controller()->set_control(button, 1.0);
        QTRY_VERIFY_WITH_TIMEOUT(window_->scene()->live().led_current.at(led) > 5e-3, 5000);
        // Structural edits are refused while running unless the run is stopped.
        window_->run_controller()->set_control(button, 0.0);
        QTRY_VERIFY_WITH_TIMEOUT(window_->scene()->live().led_current.at(led) < 1e-6, 5000);
        const auto status = window_->run_controller()->status();
        QVERIFY(status.sim_time > 0.2);
        window_->stop();
        QVERIFY(!window_->run_controller()->active());
    }

    void transient_run_opens_the_record_in_the_analyzer()
    {
        if (!engine_available()) {
            QSKIP("No ngspice library configured for this build.");
        }
        const model::Id v = place("vdc", {0, 0});
        label(v, "+", "IN");
        label(v, "-", "0");
        const model::Id r = place("resistor", {20, 0});
        label(r, "1", "IN");
        label(r, "2", "OUT");
        const model::Id c = place("capacitor", {40, 0});
        doc().apply(editing::set_parameter(doc().schematic(), c, "C", "1u"));
        label(c, "1", "OUT");
        label(c, "2", "0");
        model::Probe probe;
        probe.target = {c, "1", {}};
        doc().apply(editing::add_probe(doc().id_source(), probe));
        auto settings = doc().schematic().settings();
        settings.mode = model::SimulationMode::Transient;
        settings.transient_stop = 5e-3;
        settings.transient_max_step = 1e-6;
        doc().apply(editing::set_settings(doc().schematic(), settings));
        window_->run();
        QTRY_COMPARE_WITH_TIMEOUT(window_->run_controller()->state(), sim::RunState::Finished, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(window_->analyzer()->isVisible(), 2000);
        const auto& channels = window_->run_controller()->channels();
        QCOMPARE(channels.size(), std::size_t{1});
        QCOMPARE(channels[0].name, QStringLiteral("V(OUT)"));
        window_->analyzer()->refresh();
        const auto& traces = window_->analyzer()->scope()->traces();
        QCOMPARE(traces.size(), std::size_t{1});
        QVERIFY(traces[0].samples.size() > 1000);
        QVERIFY(std::abs(traces[0].samples.back().value - 5.0 * (1 - std::exp(-5.0))) < 0.05);
        QVERIFY(window_->analyzer()->measurement_row()->text().contains(QStringLiteral("max")));
        const QString csv = dir_.filePath(QStringLiteral("out.csv"));
        QString error;
        QVERIFY2(window_->analyzer()->export_csv(csv, error), qPrintable(error));
        QVERIFY(QFileInfo(csv).size() > 1000);
        QFile exported(csv);
        QVERIFY(exported.open(QIODevice::ReadOnly | QIODevice::Text));
        QVERIFY(exported.readLine().startsWith("# SimNodus")); // how the data was computed (SIM-09)
        QVERIFY(exported.readLine().startsWith("time_s,V(OUT)_V"));
    }

    void blue_pill_example_runs_its_firmware_and_resets()
    {
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY) || !defined(SIMNODUS_TEST_RENODE)
        QSKIP("No ngspice or Renode configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/07_Blue_Pill_blink.snx"), errors, warnings));
        model::Id board = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            if (c.type == "bluepill") {
                board = c.id;
            }
        }
        QVERIFY(board != model::no_id);
        QVERIFY(doc().base_dir().endsWith(QStringLiteral("examples")));

        // The firmware field shows the path relative to the example's folder.
        window_->scene()->select_ids({board});
        auto* firmware = window_->properties()->findChild<QLineEdit*>(QStringLiteral("file_firmware"));
        QTRY_VERIFY(firmware != nullptr || (firmware = window_->properties()->findChild<QLineEdit*>(
                                                QStringLiteral("file_firmware"))) != nullptr);
        QCOMPARE(firmware->text(), QStringLiteral("firmware/blink/blink.elf"));
        QVERIFY(window_->properties()->findChild<QPushButton*>(QStringLiteral("reload_firmware")));

        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        // Renode starts first; then the pins appear on the board.
        QTRY_VERIFY_WITH_TIMEOUT(!window_->run_controller()->status().starting_mcu
                && window_->scene()->live().mcu_pins.count(board)
                && window_->scene()->live().mcu_pins.at(board).at("PC13").mode == mcu::PinMode::Output,
            60000);
        // PC13 low lights the on-board LED, high turns it off: both within a second.
        QTRY_VERIFY_WITH_TIMEOUT(!window_->scene()->live().mcu_pins.at(board).at("PC13").level, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(window_->scene()->live().mcu_pins.at(board).at("PC13").level, 5000);

        // The reset button on the board restarts the firmware.
        view()->fit_content();
        QTest::mouseClick(view()->viewport(), Qt::LeftButton, Qt::NoModifier, at({0, -11}));
        QTRY_COMPARE_WITH_TIMEOUT(window_->run_controller()->mcu_status(board)->resets, 1, 5000);
        QVERIFY(window_->run_controller()->active());
        window_->stop();
        QVERIFY(!window_->run_controller()->active());
#endif
    }

    void alt_click_sets_a_differential_reference()
    {
        const model::Id r = place("resistor", {0, 0});
        view()->set_tool(Tool::Probe);
        QTest::mouseClick(view()->viewport(), Qt::LeftButton, Qt::ShiftModifier, at({0, -3})); // voltage at pin 1
        QTest::mouseClick(view()->viewport(), Qt::LeftButton, Qt::AltModifier, at({0, 3}));    // relative to pin 2
        QCOMPARE(doc().schematic().probes().size(), std::size_t{1});
        const auto& p = doc().schematic().probes()[0];
        QCOMPARE(p.kind, model::ProbeKind::Voltage);
        QVERIFY(p.reference.has_value());
        QCOMPARE(p.reference->item, r);
        QCOMPARE(p.reference->pin, std::string("2"));
    }
};

QTEST_MAIN(EditorTests)
#include "editor_tests.moc"
