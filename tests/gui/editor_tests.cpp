// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// End-to-end tests of the Circuit Editor and the run controls (SN-112..SN-122),
// driven through the real main window, offscreen.
#include "analyzer_window.hpp"
#include "app_settings.hpp"
#include "document.hpp"
#include "logic_widget.hpp"
#include "main_window.hpp"
#include "messages_panel.hpp"
#include "properties_panel.hpp"
#include "run_controller.hpp"
#include "schematic_scene.hpp"
#include "schematic_view.hpp"
#include "theme.hpp"
#include "uart_terminal.hpp"

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "editing/operations.hpp"
#include "io/project_file.hpp"
#include "netlist/connectivity.hpp"
#include "sim/engine_host.hpp"
#endif

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPointer>
#include <QSettings>
#include <QSpinBox>
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

    void buses_and_splitters_edit_in_the_properties_panel()
    {
        model::Id bus = model::no_id;
        model::Id splitter = model::no_id;
        doc().apply(editing::add_bus(doc().id_source(), {{0, 0}, {20, 0}}, "D", &bus));
        doc().apply(editing::add_splitter(doc().id_source(), {20, 0}, "D", 0, 4, &splitter));
        QVERIFY(window_->scene()->item_of(bus));
        QVERIFY(window_->scene()->item_of(splitter));
        QCOMPARE(window_->scene()->element_at(view()->mapToScene(at({10, 0}))), bus);
        // Bit 2's pin is a named net: a wire may end there.
        QCOMPARE(window_->scene()->net_name_at({22, 4}).value_or(""), std::string("D2"));
        window_->scene()->select_ids({bus});
        auto* name = window_->properties()->findChild<QLineEdit*>(QStringLiteral("busName"));
        QVERIFY(name);
        name->setText(QStringLiteral("DATA"));
        emit name->editingFinished();
        QCOMPARE(doc().schematic().bus(bus)->name, std::string("DATA"));
        QCOMPARE(doc().schematic().splitter(splitter)->bus, std::string("DATA")); // follows the bus
        window_->scene()->select_ids({splitter});
        auto* count = window_->properties()->findChild<QSpinBox*>(QStringLiteral("splitterCount"));
        QVERIFY(count);
        count->setValue(2);
        emit count->editingFinished();
        QCOMPARE(doc().schematic().splitter(splitter)->count, 2);
        doc().undo_stack()->undo();
        QCOMPARE(doc().schematic().splitter(splitter)->count, 4);
    }

    void dragging_a_potentiometer_moves_the_wiper_with_the_mouse()
    {
        // Upside down (pin 1 at the bottom, as in examples 14-16): dragging up must
        // move the arrow up, that is towards 100 %.
        const model::Id rv = place("potentiometer", {0, 0}, {180, false});
        double last = -1;
        connect(view(), &SchematicView::control_changed, this, [&last](model::Id, double value) { last = value; });
        view()->set_running(true);
        QTest::mousePress(view()->viewport(), Qt::LeftButton, Qt::NoModifier, at({0, 0}));
        move_to({0, -4}, Qt::LeftButton);
        QTest::mouseRelease(view()->viewport(), Qt::LeftButton, Qt::NoModifier, at({0, -4}));
        view()->set_running(false);
        QVERIFY2(last > 0.0, qPrintable(QStringLiteral("value %1").arg(last)));
        (void)rv;
    }

    void potentiometer_shows_its_position()
    {
        const model::Id rv = place("potentiometer", {0, 0});
        auto* item = window_->scene()->item_of(rv);
        QVERIFY(item);
        const QRectF at_half = item->boundingRect(); // "50 %" left of the body
        doc().apply(editing::set_parameter(doc().schematic(), rv, "position", "100"));
        const QRectF at_full = item->boundingRect(); // "100 %", wider and lower
        QVERIFY(at_full.left() < at_half.left());
        QVERIFY(at_full.bottom() >= at_half.bottom());
    }

    void model_list_offers_built_in_and_folder_models()
    {
        // A models folder with a user's NPN (and a line that is not a model).
        const QString folder = dir_.filePath(QStringLiteral("models"));
        QVERIFY(QDir().mkpath(folder));
        QFile file(folder + QStringLiteral("/mine.lib"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("* MY-NPN: a test transistor\n.model MY-NPN NPN(BF=50)\nR1 1 2 1k\n");
        file.close();
        qputenv("SIMNODUS_MODELS_DIR", folder.toUtf8());
        const model::Id q = place("npn", {0, 0});
        window_->scene()->select_ids({q});
        auto* box = window_->properties()->findChild<QComboBox*>(QStringLiteral("parameter_model"));
        QVERIFY(box);
        QCOMPARE(box->currentText(), QStringLiteral("Generic"));
        QVERIFY(box->findText(QStringLiteral("2N2222A")) >= 0);
        QVERIFY(box->findText(QStringLiteral("1N4148")) < 0); // diodes are not offered for an NPN
        const int mine = box->findText(QStringLiteral("MY-NPN"));
        QVERIFY(mine >= 0);
        QVERIFY(box->itemData(mine, Qt::ToolTipRole).toString().contains(QStringLiteral("mine.lib")));
        emit box->activated(mine);
        const auto* c = doc().schematic().component(q);
        QCOMPARE(c->parameters.at("model"), std::string("MY-NPN"));
        QCOMPARE(c->models.at("model"), std::string(".model MY-NPN NPN(BF=50)"));
        // Back to a built-in model: nothing is kept in the component.
        box = window_->properties()->findChild<QComboBox*>(QStringLiteral("parameter_model"));
        emit box->activated(box->findText(QStringLiteral("2N3904")));
        QVERIFY(doc().schematic().component(q)->models.empty());
        qunsetenv("SIMNODUS_MODELS_DIR");
    }

    void text_notes_show_move_and_edit_in_the_properties_panel()
    {
        model::Id note = model::no_id;
        doc().apply(editing::add_note(doc().id_source(), {4, 4}, "Close SW1.\nThen watch the LED.", &note));
        QVERIFY(window_->scene()->item_of(note));
        // Clicking the text selects the note; dragging it moves it on the grid.
        const QPoint start = at({6, 5});
        QCOMPARE(window_->scene()->element_at(view()->mapToScene(start)), note);
        QTest::mousePress(view()->viewport(), Qt::LeftButton, Qt::NoModifier, start);
        move_to({9, 7}, Qt::LeftButton);
        QTest::mouseRelease(view()->viewport(), Qt::LeftButton, Qt::NoModifier, at({9, 7}));
        QCOMPARE(doc().schematic().note(note)->position, (model::Point{7, 6}));
        window_->scene()->select_ids({note});
        auto* text = window_->properties()->findChild<QPlainTextEdit*>(QStringLiteral("noteText"));
        QVERIFY(text);
        QCOMPARE(text->toPlainText(), QStringLiteral("Close SW1.\nThen watch the LED."));
        text->setPlainText(QStringLiteral("Press the button."));
        auto* apply = window_->properties()->findChild<QPushButton*>(QStringLiteral("noteApply"));
        QVERIFY(apply);
        apply->click();
        QCOMPARE(doc().schematic().note(note)->text, std::string("Press the button."));
        doc().undo_stack()->undo();
        QCOMPARE(doc().schematic().note(note)->text, std::string("Close SW1.\nThen watch the LED."));
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
        QVERIFY(window_->scene()->live().glow.count(led));
        QVERIFY(window_->scene()->live().glow.at(led).level < 1e-4); // button released: dark
        window_->run_controller()->set_control(button, 1.0);
        QTRY_VERIFY_WITH_TIMEOUT(window_->scene()->live().glow.at(led).level > 0.5, 5000);
        // Structural edits are refused while running unless the run is stopped.
        window_->run_controller()->set_control(button, 0.0);
        QTRY_VERIFY_WITH_TIMEOUT(window_->scene()->live().glow.at(led).level < 1e-4, 5000);
        const auto status = window_->run_controller()->status();
        QVERIFY(status.sim_time > 0.2);
        window_->stop();
        QVERIFY(!window_->run_controller()->active());
    }

    void scope_zero_marker_drag_holds_in_a_live_run()
    {
        // Owner's report (2026-10-18): dragging the selected channel's 0 V marker in a
        // live run snapped back. The offset must stay where the drag left it.
        if (!engine_available()) {
            QSKIP("No ngspice library configured for this build.");
        }
        const model::Id v = place("vpulse", {0, 0});
        label(v, "+", "IN");
        label(v, "-", "0");
        const model::Id r = place("resistor", {20, 0});
        label(r, "1", "IN");
        label(r, "2", "OUT");
        const model::Id c = place("capacitor", {40, 0});
        label(c, "1", "OUT");
        label(c, "2", "0");
        for (const model::Id item : {v, c}) {
            model::Probe probe;
            probe.target = {item, item == v ? "+" : "1", {}};
            doc().apply(editing::add_probe(doc().id_source(), probe));
        }
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        window_->show_analyzer();
        auto* analyzer = window_->analyzer();
        auto* scope = analyzer->scope();
        scope->resize(700, 400);
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->status().sim_time > 1.5, 20000); // after the first autoscale
        QTest::qWait(300);
        scope->set_selected_trace(1);
        const QRectF plot = scope->plot_rect();
        const auto& before = scope->traces();
        QCOMPARE(before.size(), std::size_t{2});
        const double upp = before[1].per_division * ScopeWidget::vertical_divisions / plot.height();
        const QPointF marker(plot.left() - 3, std::clamp(plot.center().y() + before[1].offset / upp, plot.top(), plot.bottom()));
        auto send = [scope](QEvent::Type type, QPointF at, Qt::MouseButtons buttons) {
            QMouseEvent event(type, at, scope->mapToGlobal(at), Qt::LeftButton, buttons, Qt::NoModifier);
            QApplication::sendEvent(scope, &event);
        };
        const double division = plot.height() / ScopeWidget::vertical_divisions;
        const double start = before[1].offset;
        send(QEvent::MouseButtonPress, marker, Qt::LeftButton);
        for (int i = 1; i <= 10; ++i) {
            send(QEvent::MouseMove, marker - QPointF(0, i * division / 5), Qt::LeftButton); // up two divisions
            QTest::qWait(30);
        }
        send(QEvent::MouseButtonRelease, marker - QPointF(0, 2 * division), Qt::NoButton);
        const double expected = start - 2 * scope->traces()[1].per_division;
        QCOMPARE(scope->traces()[1].offset, expected);
        QTest::qWait(500); // several refreshes of the live display
        QCOMPARE(scope->selected_trace(), 1);
        QVERIFY2(std::abs(scope->traces()[1].offset - expected) < 1e-9,
            qPrintable(QStringLiteral("offset %1, expected %2").arg(scope->traces()[1].offset).arg(expected)));
        window_->stop();
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
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
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
        // The board starts first; then the pins appear on it.
        QTRY_VERIFY_WITH_TIMEOUT(!window_->run_controller()->status().starting_mcu
                && window_->scene()->live().mcu_pins.count(board)
                && window_->scene()->live().mcu_pins.at(board).at("PC13").mode == mcu::PinMode::Output,
            150000);
        // PC13 low lights the on-board LED, high turns it off: both within a second.
        QTRY_VERIFY_WITH_TIMEOUT(!window_->scene()->live().mcu_pins.at(board).at("PC13").level, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(window_->scene()->live().mcu_pins.at(board).at("PC13").level, 5000);
        // PA0 drives the external LED through 330 Ohm (SN-132): it blinks at 2 Hz.
        model::Id led = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            if (c.type == "led") {
                led = c.id;
            }
        }
        auto led_current = [this, led] {
            const auto& live = window_->scene()->live().glow; // level x 10 mA = amperes
            const auto it = live.find(led);
            return it == live.end() ? 0.0 : it->second.level * 0.01;
        };
        QTRY_VERIFY_WITH_TIMEOUT(led_current() > 2e-3, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(led_current() < 1e-4, 5000);

        // The reset button on the board restarts the firmware.
        view()->fit_content();
        QTest::mouseClick(view()->viewport(), Qt::LeftButton, Qt::NoModifier, at({0, -11}));
        QTRY_COMPARE_WITH_TIMEOUT(window_->run_controller()->mcu_status(board)->resets, 1, 5000);
        QVERIFY(window_->run_controller()->active());
        window_->stop();
        QVERIFY(!window_->run_controller()->active());
#endif
    }

    void blue_pill_button_triggers_the_interrupt()
    {
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/08_Blue_Pill_button_interrupt.snx"), errors, warnings));
        model::Id button = model::no_id;
        model::Id led = model::no_id;
        model::Id board = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            button = c.type == "button_no" ? c.id : button;
            led = c.type == "led" ? c.id : led;
            board = c.type == "bluepill" ? c.id : board;
        }
        auto led_current = [this, led] {
            const auto& live = window_->scene()->live().glow; // level x 10 mA = amperes
            const auto it = live.find(led);
            return it == live.end() ? 0.0 : it->second.level * 0.01;
        };
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(!window_->run_controller()->status().starting_mcu
                && window_->scene()->live().mcu_pins.count(board)
                && window_->scene()->live().mcu_pins.at(board).at("PA1").mode == mcu::PinMode::Input,
            150000);
        // The internal pull-up gives the open button's input a 1.
        QTRY_VERIFY_WITH_TIMEOUT(window_->scene()->live().mcu_pins.at(board).at("PA1").level, 5000);
        QVERIFY(led_current() < 1e-4);
        window_->run_controller()->set_control(button, 1.0); // press: EXTI1 toggles PA0
        QTRY_VERIFY_WITH_TIMEOUT(led_current() > 2e-3, 5000);
        QVERIFY(!window_->scene()->live().mcu_pins.at(board).at("PA1").level);
        window_->run_controller()->set_control(button, 0.0);
        QTest::qWait(300);
        QVERIFY(led_current() > 2e-3); // release keeps it on
        window_->run_controller()->set_control(button, 1.0);
        QTRY_VERIFY_WITH_TIMEOUT(led_current() < 1e-4, 5000);
        window_->stop();
#endif
    }

    void uart_terminal_talks_to_the_firmware()
    {
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/09_Blue_Pill_serial_terminal.snx"), errors, warnings));
        model::Id led = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            led = c.type == "led" ? c.id : led;
        }
        auto led_current = [this, led] {
            const auto& live = window_->scene()->live().glow; // level x 10 mA = amperes
            const auto it = live.find(led);
            return it == live.end() ? 0.0 : it->second.level * 0.01;
        };
        window_->show_analyzer();
        UartTerminal* terminal = window_->analyzer()->terminal();
        terminal->timestamps()->setChecked(true);
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(terminal->ports()->count() == 3, 150000);
        QVERIFY(terminal->ports()->currentText().contains(QStringLiteral("USART1")));
        QTRY_VERIFY_WITH_TIMEOUT(terminal->output()->toPlainText().contains(QStringLiteral("Blue Pill ready")), 150000);
        QTRY_VERIFY_WITH_TIMEOUT(terminal->output()->toPlainText().contains(QStringLiteral("tick 1")), 10000);
        // Time stamps: "tick 1" is printed at about 1 s of simulated time (1.000021 s).
        QVERIFY(terminal->output()->toPlainText().contains(QStringLiteral("[1.00")));
        QVERIFY(terminal->output()->toPlainText().contains(QStringLiteral(" s] tick 1")));
        QVERIFY(led_current() < 1e-4);
        terminal->input()->setText(QStringLiteral("1"));
        terminal->send();
        QTRY_VERIFY_WITH_TIMEOUT(led_current() > 2e-3, 5000);
        terminal->input()->setText(QStringLiteral("0"));
        QTest::keyClick(terminal->input(), Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(led_current() < 1e-4, 5000);
        window_->stop();
#endif
    }

    void logic_lanes_show_the_pins_that_change()
    {
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/07_Blue_Pill_blink.snx"), errors, warnings));
        window_->show_analyzer();
        AnalyzerWindow* analyzer = window_->analyzer();
        analyzer->timebase()->setCurrentIndex(analyzer->timebase()->findData(0.5)); // 500 ms/div
        window_->run();
        // 32 pin lanes plus V(PA0); the blinking pins appear without being chosen.
        QTRY_VERIFY_WITH_TIMEOUT(analyzer->lane_list()->count() == 33, 150000);
        auto lane_index = [analyzer](const QString& title) {
            const auto& lanes = analyzer->logic()->lanes();
            for (std::size_t i = 0; i < lanes.size(); ++i) {
                if (lanes[i].title == title) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        };
        QTRY_VERIFY_WITH_TIMEOUT(lane_index(QStringLiteral("U1.PC13")) >= 0 && lane_index(QStringLiteral("U1.PA3")) >= 0,
            60000); // PA3 first changes at 4 s of simulated time
        QCOMPARE(analyzer->lane_list()->item(0)->text(), QStringLiteral("U1.PA0")); // natural pin order
        QVERIFY(lane_index(QStringLiteral("U1.PB12")) < 0); // unused pins stay hidden
        const int pc13 = lane_index(QStringLiteral("U1.PC13"));
        QTRY_VERIFY_WITH_TIMEOUT(analyzer->logic()->visible_transitions(pc13).size() >= 2, 60000);
        analyzer->logic()->set_selected(pc13);
        QVERIFY(analyzer->transitions()->text().contains(QStringLiteral("U1.PC13")));
        QVERIFY(analyzer->transitions()->text().contains(QStringLiteral("transition")));
        // Only the checked lanes go to the VCD file (AN-10).
        const QString vcd = dir_.filePath(QStringLiteral("lanes.vcd"));
        QString error;
        QVERIFY2(analyzer->export_vcd(vcd, error), qPrintable(error));
        QFile exported(vcd);
        QVERIFY(exported.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(exported.readAll());
        QVERIFY(text.contains(QStringLiteral("$timescale 1 ns $end")));
        QVERIFY(text.contains(QStringLiteral(" U1_PC13 $end")));
        QVERIFY(text.contains(QStringLiteral(" U1_PA3 $end")));
        QVERIFY(!text.contains(QStringLiteral("U1_PB12")));
        QVERIFY(!text.contains(QStringLiteral("V_PA0")));
        QVERIFY(text.count(QLatin1Char('#')) >= 3); // dump start and at least two changes
        window_->stop();
#endif
    }

    void probes_are_dragged_to_another_wire_or_pin()
    {
        const model::Id r1 = place("resistor", {0, 0}); // pins at (0,-3) and (0,3)
        const model::Id r2 = place("resistor", {10, 0});
        model::Id wire = model::no_id;
        doc().apply(editing::add_wire(doc().id_source(), {{0, 3}, {0, 6}, {10, 6}, {10, 3}}, &wire));
        model::Probe voltage;
        voltage.kind = model::ProbeKind::Voltage;
        voltage.target = {r1, "1", {0, -3}};
        voltage.color = "#1f77b4";
        model::Id probe = model::no_id;
        doc().apply(editing::add_probe(doc().id_source(), voltage, &probe));
        model::Probe current = voltage;
        current.kind = model::ProbeKind::Current;
        current.target = {r2, "1", {10, -3}};
        model::Id current_probe = model::no_id;
        doc().apply(editing::add_probe(doc().id_source(), current, &current_probe));

        auto drag = [this](model::Id id, model::Point to) {
            const QPoint from = view()->mapFromScene(window_->scene()->item_of(id)->sceneBoundingRect().center());
            QTest::mousePress(view()->viewport(), Qt::LeftButton, Qt::NoModifier, from);
            move_to(to, Qt::LeftButton);
            QTest::mouseRelease(view()->viewport(), Qt::LeftButton, Qt::NoModifier, at(to));
        };
        // A voltage probe moves from R1's pin to a point on the wire.
        drag(probe, {5, 6});
        const auto* moved = doc().schematic().probe(probe);
        QVERIFY(moved);
        QCOMPARE(moved->target.item, wire);
        QVERIFY(moved->target.pin.empty());
        QCOMPARE(moved->kind, model::ProbeKind::Voltage);
        QCOMPARE(moved->color, std::string("#1f77b4"));
        doc().undo_stack()->undo();
        QCOMPARE(doc().schematic().probe(probe)->target.item, r1);

        // A current probe refuses a wire but moves to another pin.
        drag(current_probe, {5, 6});
        QCOMPARE(doc().schematic().probe(current_probe)->target.item, r2);
        drag(current_probe, {10, 3});
        QCOMPARE(doc().schematic().probe(current_probe)->target.pin, std::string("2"));
        QCOMPARE(doc().schematic().probe(current_probe)->kind, model::ProbeKind::Current);
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
