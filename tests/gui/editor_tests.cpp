// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// End-to-end tests of the Circuit Editor and the run controls (SN-112..SN-122),
// driven through the real main window, offscreen.
#include "analyzer_window.hpp"
#include "app_settings.hpp"
#include "document.hpp"
#include "logic_widget.hpp"
#include "user_models.hpp"
#include "component_panel.hpp"
#include "control_server.hpp"
#include "io_link.hpp"
#include "main_window.hpp"
#include "memory_window.hpp"
#include "messages_panel.hpp"
#include "properties_panel.hpp"
#include "run_controller.hpp"
#include "schematic_scene.hpp"
#include "schematic_view.hpp"
#include "theme.hpp"
#include "uart_terminal.hpp"

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "automation/automation.hpp"
#include "editing/operations.hpp"
#include "io/project_file.hpp"
#include "netlist/connectivity.hpp"
#include "sim/engine_host.hpp"
#endif

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QProcess>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocalSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QPointer>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QUndoStack>

#include <algorithm>
#include <cmath>
#include <map>
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

    // A request through the window's external control (SN-216); the result, or null on failure.
    automation::json control(const std::string& command, automation::json args = automation::json::object())
    {
        last_reply_ = window_->control()->execute({{"command", command}, {"args", std::move(args)}});
        return last_reply_.value("ok", false) ? last_reply_["result"] : automation::json(nullptr);
    }
    automation::json last_reply_;

    // 100 Hz square wave into an RC low-pass, built through external control.
    void control_rc()
    {
        QVERIFY(control("component.add", {{"type", "vpulse"}, {"at", {0, 0}}, {"parameters", {{"f", "100"}}}}).is_object());
        QVERIFY(control("component.add",
            {{"type", "resistor"}, {"at", {6, -6}}, {"rotation", 270}, {"parameters", {{"R", "1k"}}}}).is_object());
        QVERIFY(control("component.add", {{"type", "capacitor"}, {"at", {12, 0}}, {"parameters", {{"C", "1u"}}}}).is_object());
        QVERIFY(control("component.add", {{"type", "ground"}, {"attach", {{"to", "V1.-"}}}}).is_object());
        QVERIFY(control("wire.connect", {{"from", "V1.+"}, {"to", "R1.1"}}).is_object());
        QVERIFY(control("wire.connect", {{"from", "R1.2"}, {"to", "C1.1"}}).is_object());
        QVERIFY(control("wire.connect", {{"from", "C1.2"}, {"to", "V1.-"}}).is_object());
        QVERIFY(control("label.add", {{"name", "OUT"}, {"at", "C1.1"}}).is_object());
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

    void properties_panel_offers_the_computers_serial_ports()
    {
        // SN-237: the Port of a Serial Port part lists the computer's ports and
        // loopback, and takes a typed name too.
        const model::Id j = place("serial_port", {0, 0});
        window_->scene()->select_ids({j});
        auto* box = window_->properties()->findChild<QComboBox*>(QStringLiteral("parameter_port"));
        QVERIFY(box);
        QVERIFY(box->isEditable());
        QVERIFY(box->findText(QStringLiteral("loopback")) >= 0);
        QCOMPARE(box->currentText(), QStringLiteral("loopback"));
        box->lineEdit()->setFocus();
        box->lineEdit()->selectAll();
        QTest::keyClicks(box->lineEdit(), QStringLiteral("COM7"));
        QTest::keyClick(box->lineEdit(), Qt::Key_Return);
        QCOMPARE(doc().schematic().component(j)->parameters.at("port"), std::string("COM7"));
        auto* parity = window_->properties()->findChild<QComboBox*>(QStringLiteral("parameter_parity"));
        QVERIFY(parity);
        QCOMPARE(parity->currentData().toString(), QStringLiteral("None"));
    }

    void buses_and_splitters_edit_in_the_properties_panel()
    {
        model::Id bus = model::no_id;
        model::Id splitter = model::no_id;
        doc().apply(editing::add_bus(doc().id_source(), {{0, 0}, {20, 0}}, "D", &bus));
        doc().apply(editing::add_splitter(doc().id_source(), {20, 0}, "D", 0, {4}, {1, 1, 1, 1}, &splitter));
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
        auto* outputs = window_->properties()->findChild<QLineEdit*>(QStringLiteral("splitterOutputs"));
        QVERIFY(outputs);
        QCOMPARE(outputs->text(), QStringLiteral("4*1"));
        outputs->setText(QStringLiteral("2,2"));
        emit outputs->editingFinished();
        QCOMPARE(doc().schematic().splitter(splitter)->outputs, (std::vector<int>{2, 2}));
        doc().undo_stack()->undo();
        QCOMPARE(doc().schematic().splitter(splitter)->outputs, (std::vector<int>{1, 1, 1, 1}));
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

    void dragging_an_adjustable_source_up_raises_it()
    {
        // Owner's report of 2026-10-25: up lowered the value, against the wheel.
        place("vadj", {0, 0});
        double last = -1;
        connect(view(), &SchematicView::control_changed, this, [&last](model::Id, double value) { last = value; });
        view()->set_running(true);
        QTest::mousePress(view()->viewport(), Qt::LeftButton, Qt::NoModifier, at({0, 0}));
        move_to({0, -4}, Qt::LeftButton);
        QTest::mouseRelease(view()->viewport(), Qt::LeftButton, Qt::NoModifier, at({0, -4}));
        view()->set_running(false);
        QVERIFY2(last > 0.0, qPrintable(QStringLiteral("value %1").arg(last)));
    }

    void messages_can_be_copied()
    {
        auto* messages = window_->messages();
        messages->clear();
        messages->add(Severity::Warning, QStringLiteral("first"), QStringLiteral("check"));
        messages->add(Severity::Error, QStringLiteral("second"));
        const QStringList all = messages->text(false).split(QLatin1Char('\n'));
        QCOMPARE(all.size(), 2);
        QVERIFY(all[0].endsWith(QStringLiteral(": first (check)")));
        QVERIFY(all[1].endsWith(QStringLiteral(": second")));
        QVERIFY(messages->text(true).isEmpty()); // nothing selected
        auto* list = messages->findChild<QTreeWidget*>(QStringLiteral("messageList"));
        QVERIFY(list);
        list->topLevelItem(1)->setSelected(true);
        QVERIFY(messages->text(true).endsWith(QStringLiteral(": second")));
        list->setFocus();
        QTest::keyClick(list, Qt::Key_C, Qt::ControlModifier);
        QVERIFY(QApplication::clipboard()->text().endsWith(QStringLiteral(": second")));
        messages->clear();
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

    void op_amp_offers_vendor_subcircuits_from_the_models_folder()
    {
        const QString folder = dir_.filePath(QStringLiteral("subckt-models"));
        QVERIFY(QDir().mkpath(folder));
        QFile file(folder + QStringLiteral("/amps.lib"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("* MYAMP: a test op-amp\n.SUBCKT MYAMP 1 2 3 4 5\nE1 5 0 1 2 1E5\nR1 3 4 1MEG\n.ENDS\n"
                   ".SUBCKT TWO 1 2\nR1 1 2 1k\n.ENDS\n");
        file.close();
        qputenv("SIMNODUS_MODELS_DIR", folder.toUtf8());
        const model::Id u = place("opamp", {0, 0});
        window_->scene()->select_ids({u});
        auto* box = window_->properties()->findChild<QComboBox*>(QStringLiteral("parameter_model"));
        QVERIFY(box);
        QCOMPARE(box->currentText(), QStringLiteral("Behavioral"));
        QVERIFY(window_->properties()->findChild<QPushButton*>(QStringLiteral("modelPins")) == nullptr);
        const int amp = box->findText(QStringLiteral("MYAMP"));
        QVERIFY(amp >= 0);
        QVERIFY(box->findText(QStringLiteral("TWO")) < 0); // two ports do not fit an op-amp
        QVERIFY(box->itemData(amp, Qt::ToolTipRole).toString().contains(QStringLiteral("ports 1 2 3 4 5")));
        emit box->activated(amp);
        const auto* c = doc().schematic().component(u);
        QCOMPARE(c->parameters.at("model"), std::string("MYAMP"));
        QVERIFY(c->models.at("model").find(".SUBCKT MYAMP") != std::string::npos);
        // Now a subcircuit: its pins can be assigned.
        QTRY_VERIFY(window_->properties()->findChild<QPushButton*>(QStringLiteral("modelPins")) != nullptr);
        qunsetenv("SIMNODUS_MODELS_DIR");
    }

    void model_list_offers_built_in_and_folder_models()
    {
        // A models folder with a user's NPN (and a line that is not a model).
        const QString folder = dir_.filePath(QStringLiteral("models"));
        QVERIFY(QDir().mkpath(folder));
        QFile file(folder + QStringLiteral("/mine.lib"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("* MY-NPN: a test transistor\n.model MY-NPN NPN(BF=50)\nR1 1 2 1k\n"
                   ".model ODD NPN(BF=80 FOO=2)\n.subckt AMP 1 2\n.ends\n");
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
        QVERIFY(box->findText(QStringLiteral("ODD")) >= 0); // kept, without FOO
        // What was left out or skipped is in the Messages panel, once per read.
        QCOMPARE(window_->messages()->warning_count(), 1);
        const auto& issues = user_model_issues();
        QCOMPARE(static_cast<int>(issues.size()), 1);
        QCOMPARE(issues[0], QStringLiteral("models/mine.lib, line 4, ODD: left out FOO=2 (not a NPN parameter)"));
        QVERIFY(box->findText(QStringLiteral("AMP")) < 0); // a two-pin subcircuit does not fit an NPN
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

    void logic_cursors_are_dragged_like_the_scope_cursors()
    {
        LogicWidget logic;
        logic.resize(600, 120);
        logic.set_lanes({{QStringLiteral("CLK"), Qt::blue, false, {{0.2, true}, {0.4, false}, {0.6, true}}}});
        logic.set_window(0.0, 1.0);
        logic.set_cursors(true, 0.3, 0.7);
        logic.show();
        QVERIFY(QTest::qWaitForWindowExposed(&logic));
        std::vector<std::pair<int, double>> moved;
        connect(&logic, &LogicWidget::cursor_moved, this, [&moved](int i, double t) { moved.push_back({i, t}); });
        // The plot starts after the 96-pixel lane names and ends 8 pixels before the edge.
        const double width = 600 - 96 - 8;
        const QPoint a(static_cast<int>(96 + 0.3 * width), 40);
        const QPoint target(static_cast<int>(96 + 0.5 * width), 40);
        QTest::mousePress(&logic, Qt::LeftButton, {}, a);
        QMouseEvent move(QEvent::MouseMove, QPointF(target), logic.mapToGlobal(QPointF(target)), Qt::NoButton,
            Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&logic, &move);
        QTest::mouseRelease(&logic, Qt::LeftButton, {}, target);
        QVERIFY(!moved.empty());
        QCOMPARE(moved.back().first, 0);
        QVERIFY(std::abs(moved.back().second - 0.5) < 0.01);
    }

    void model_folders_are_chosen_and_all_read()
    {
        QTemporaryDir one;
        QTemporaryDir two;
        auto write = [](const QString& path, const char* text) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(text);
        };
        write(one.filePath(QStringLiteral("a.lib")), ".model FOLDER-ONE NPN(BF=50)\n");
        write(two.filePath(QStringLiteral("b.lib")), ".model FOLDER-TWO NPN(BF=80)\n");
        window_->set_model_folders({one.path(), two.path()});
        QCOMPARE(user_models_folders(), (QStringList{one.path(), two.path()}));
        QCOMPARE(settings_->value(QStringLiteral("models/folders")).toStringList(), (QStringList{one.path(), two.path()}));
        QStringList names;
        for (const auto& m : user_models()) {
            names << QString::fromStdString(m.model.name);
        }
        QVERIFY(names.contains(QStringLiteral("FOLDER-ONE")));
        QVERIFY(names.contains(QStringLiteral("FOLDER-TWO")));
        window_->set_model_folders({});
        QCOMPARE(user_models_folders(), QStringList{default_models_folder()});
        QVERIFY(!settings_->contains(QStringLiteral("models/folders")));
    }

    void blocks_are_placed_from_files_opened_and_updated()
    {
        // A block file: IN -R1- OUT, with two ports (PRJ-06).
        model::Schematic circuit;
        model::Id r = model::no_id;
        model::Id in = model::no_id;
        model::Id out = model::no_id;
        editing::add_component(circuit, *window_->library().find("resistor"), {0, 0}, {}, &r).apply(circuit);
        editing::add_component(circuit, *window_->library().find("port"), {-10, 0}, {}, &in).apply(circuit);
        editing::add_component(circuit, *window_->library().find("port"), {10, 0}, {}, &out).apply(circuit);
        editing::set_parameter(circuit, out, "name", "OUT").apply(circuit);
        editing::set_parameter(circuit, out, "side", "right").apply(circuit);
        const QString block_file = dir_.filePath(QStringLiteral("Stage.snx"));
        QVERIFY(io::save_project(circuit, block_file.toStdWString()).ok);
        const QString project = dir_.filePath(QStringLiteral("uses_block.snx"));
        QVERIFY(window_->save_to(project));

        QString error;
        QCOMPARE(window_->add_block_file(block_file, error), QStringLiteral("block:Stage"));
        const auto* d = doc().library().find("block:Stage");
        QVERIFY(d);
        QCOMPARE(d->pins.size(), std::size_t{2});
        QCOMPARE(doc().schematic().block("Stage")->source, std::string("Stage.snx"));
        // The parts tree lists the project's block.
        bool listed = false;
        for (QTreeWidgetItemIterator it(window_->components()->tree()); *it; ++it) {
            listed = listed || (*it)->data(0, Qt::UserRole).toString() == QStringLiteral("block:Stage");
        }
        QVERIFY(listed);
        // A project cannot hold itself.
        QVERIFY(window_->add_block_file(project, error).isEmpty());
        QVERIFY(!error.isEmpty());

        model::Id u1 = model::no_id;
        doc().apply(editing::add_component(doc().id_source(), *d, {20, 20}, {}, &u1));
        QCOMPARE(doc().schematic().component(u1)->designator, std::string("U1"));
        window_->scene()->select_ids({u1});
        QTRY_VERIFY(window_->properties()->findChild<QPushButton*>(QStringLiteral("updateBlock")));

        // The file changes: Update copies it into the project, and undo takes it back.
        editing::set_parameter(circuit, r, "R", "2k2").apply(circuit);
        QVERIFY(io::save_project(circuit, block_file.toStdWString()).ok);
        QCOMPARE(window_->changed_block_files(), QStringList{QStringLiteral("Stage")});
        window_->properties()->findChild<QPushButton*>(QStringLiteral("updateBlock"))->click();
        QVERIFY(window_->changed_block_files().isEmpty());
        QCOMPARE(doc().schematic().block("Stage")->schematic->component(r)->parameters.at("R"), std::string("2k2"));
        doc().undo_stack()->undo();
        QCOMPARE(window_->changed_block_files(), QStringList{QStringLiteral("Stage")});
        doc().undo_stack()->redo();

        // Saved and opened again, the block is still there; then a double-click opens its file.
        QVERIFY(window_->save_to(project));
        QVERIFY(window_->open_file(project));
        QVERIFY(doc().library().find("block:Stage"));
        QVERIFY(netlist::build_connectivity(doc().schematic(), doc().library()).pin_net.count({u1, "OUT"}));
        QTest::mouseDClick(view()->viewport(), Qt::LeftButton, {}, at({20, 20}));
        QCOMPARE(QFileInfo(doc().file_path()), QFileInfo(block_file));
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

    void engine_crash_fails_the_run_and_the_editor_keeps_working()
    {
        // SIM-08 (SN-151): the run happens in the engine process; when that process
        // dies, the run fails with a message and the editor and its document stay.
        if (!engine_available()) {
            QSKIP("No ngspice library configured for this build.");
        }
        const model::Id v = place("vdc", {0, 0});
        label(v, "+", "VCC");
        label(v, "-", "0");
        const model::Id r = place("resistor", {20, 0});
        label(r, "1", "VCC");
        label(r, "2", "0");
        const model::Schematic before = doc().schematic();
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        const qint64 pid = window_->run_controller()->engine_process_id();
        QVERIFY(pid > 0);
        QVERIFY(pid != QCoreApplication::applicationPid());
        const int errors = window_->messages()->error_count();
#ifdef Q_OS_WIN
        QProcess::execute(QStringLiteral("taskkill"), {QStringLiteral("/F"), QStringLiteral("/PID"), QString::number(pid)});
#else
        QProcess::execute(QStringLiteral("kill"), {QStringLiteral("-9"), QString::number(pid)});
#endif
        QTRY_COMPARE_WITH_TIMEOUT(window_->run_controller()->state(), sim::RunState::Failed, 5000);
        // The message follows at the next live refresh.
        QTRY_VERIFY_WITH_TIMEOUT(window_->messages()->error_count() > errors, 2000);
        QVERIFY(QString::fromStdString(window_->run_controller()->status().message).contains(QStringLiteral("engine")));
        QCOMPARE(doc().schematic().components(), before.components());
        // The next run starts a new engine.
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        QVERIFY(window_->run_controller()->engine_process_id() > 0);
        QVERIFY(window_->run_controller()->engine_process_id() != pid);
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
        // Both voltages have 0 V at the same height after the autoscale (SN-233).
        QCOMPARE(before[0].offset / before[0].per_division, before[1].offset / before[1].per_division);
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
        // The plot holds a thinned copy (a few points per pixel); the record keeps every step.
        QVERIFY(traces[0].samples.size() > 100);
        QVERIFY(window_->run_controller()->transient_result().time.size() > 1000);
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

    void shift_register_example_lights_its_leds_in_turn()
    {
        // SN-203: a 74HC595 on the board's pins, run in the engine process.
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/25_Shift_register_74HC595.snx"), errors, warnings));
        std::map<std::string, model::Id> leds;
        for (const auto& c : doc().schematic().components()) {
            if (c.type == "led") {
                leds[c.designator] = c.id;
            }
        }
        QCOMPARE(leds.size(), std::size_t{8});
        const int before = window_->messages()->warning_count();
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        auto glow = [&](const char* designator) {
            const auto& live = window_->scene()->live().glow;
            const auto it = live.find(leds.at(designator));
            return it == live.end() ? 0.0 : it->second.level;
        };
        // The light runs from Q0 (LED1) to Q1 (LED2), 100 ms per step.
        QTRY_VERIFY_WITH_TIMEOUT(glow("LED1") > 0.1 && glow("LED2") < 0.01, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(glow("LED2") > 0.1 && glow("LED1") < 0.01, 10000);
        QCOMPARE(window_->messages()->warning_count(), before);
        bool lane = false;
        for (const auto& l : window_->run_controller()->logic_lanes()) {
            lane = lane || l.title == QStringLiteral("U2.Q0");
        }
        QVERIFY(lane);
        window_->stop();
#endif
    }

    void led_matrix_example_shows_a_heart_through_spi()
    {
        // SN-204: SPI1 and a MAX7219 scanning an 8x8 matrix; the glow is averaged as the eye does.
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/26_LED_matrix_MAX7219_SPI.snx"), errors, warnings));
        model::Id matrix = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            matrix = c.type == "led_matrix_8x8" ? c.id : matrix;
        }
        QVERIFY(matrix != model::no_id);
        const int before = window_->messages()->warning_count();
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        const std::uint8_t heart[8] = {0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18};
        // The heart shows for the first second; its LEDs glow, the others stay dark.
        auto heart_shown = [&] {
            const auto& lights = window_->scene()->live().lights;
            const auto it = lights.find(matrix);
            if (it == lights.end() || it->second.levels.size() != 64) {
                return false;
            }
            double dimmest_lit = 1e9;
            double brightest_dark = 0;
            for (int r = 0; r < 8; ++r) {
                for (int c = 0; c < 8; ++c) {
                    const double level = it->second.levels[static_cast<std::size_t>(r * 8 + c)];
                    if ((heart[r] >> (7 - c)) & 1u) {
                        dimmest_lit = std::min(dimmest_lit, level);
                    } else {
                        brightest_dark = std::max(brightest_dark, level);
                    }
                }
            }
            return dimmest_lit > 0.002 && dimmest_lit > 10 * brightest_dark;
        };
        QTRY_VERIFY_WITH_TIMEOUT(heart_shown(), 40000); // CI runners are slower than real time here
        // Already in the first picture, at the lowest intensity (rows went missing there before 2026-10-20).
        QVERIFY(window_->run_controller()->status().sim_time < 1.0);
        QCOMPARE(window_->messages()->warning_count(), before);
        window_->stop();
#endif
    }

    void lcd_example_shows_its_text_on_the_schematic()
    {
        // SN-206: an HD44780 behind a PCF8574 on I2C; the text reaches the canvas from the engine process.
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/28_LCD_16x2_I2C_backpack.snx"), errors, warnings));
        model::Id lcd = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            lcd = c.type == "lcd_hd44780" ? c.id : lcd;
        }
        QVERIFY(lcd != model::no_id);
        const int before = window_->messages()->warning_count();
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        auto shown = [&] {
            const auto& texts = window_->scene()->live().texts;
            const auto it = texts.find(lcd);
            return it != texts.end() && it->second.startsWith(QStringLiteral("Hello, SimNodus!\nSeconds:"));
        };
        QTRY_VERIFY_WITH_TIMEOUT(shown(), 15000);
        QCOMPARE(window_->messages()->warning_count(), before);
        window_->scene()->update(); // paints the screen with its text
        QTest::qWait(50);
        window_->stop();
#endif
    }

    void temperature_changes_while_running()
    {
        // SN-207: the LM75's temperature is a live parameter; the firmware reports the new value.
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/27_I2C_EEPROM_sensor_expander.snx"), errors, warnings));
        model::Id sensor = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            sensor = c.type == "lm75" ? c.id : sensor;
        }
        QVERIFY(sensor != model::no_id);
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        auto uart_text = [&] {
            std::string text;
            for (const auto& port : window_->run_controller()->uart_ports()) {
                std::string bytes;
                window_->run_controller()->uart_received(port, 0, bytes);
                text += bytes;
            }
            return text;
        };
        QTRY_VERIFY_WITH_TIMEOUT(uart_text().find("Temperature: 23.5 C") != std::string::npos, 20000);
        doc().apply(editing::set_parameter(doc().schematic(), sensor, "temperature", "31"));
        QTRY_VERIFY_WITH_TIMEOUT(uart_text().find("Temperature: 31.0 C") != std::string::npos, 20000);
        QVERIFY(window_->run_controller()->active()); // no restart
        window_->stop();
#endif
    }

    void adjustable_sources_turn_while_running()
    {
        // SN-209: example 29; V1 and I1 are turned like potentiometers, E1 and F1 follow.
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/29_Adjustable_and_controlled_sources.snx"), errors, warnings));
        std::map<std::string, model::Id> ids;
        for (const auto& c : doc().schematic().components()) {
            ids[c.designator] = c.id;
        }
        const int before = window_->messages()->warning_count();
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        auto readout = [&](const char* designator) {
            const auto& r = window_->scene()->live().readouts;
            const auto it = r.find(ids[designator]);
            return it == r.end() ? QString() : it->second;
        };
        // 40 % of 0-5 V is 2 V; E1 doubles it. I1 at 50 % of 20 mA, F1 x5.
        QTRY_COMPARE_WITH_TIMEOUT(readout("VM1"), QStringLiteral("4V"), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(readout("AM1"), QStringLiteral("50mA"), 10000);
        QVERIFY(window_->scene()->live().glow.count(ids["LED1"]));
        window_->run_controller()->set_control(ids["V1"], 1.0);
        window_->run_controller()->set_control(ids["I1"], 0.1);
        QTRY_COMPARE_WITH_TIMEOUT(readout("VM1"), QStringLiteral("10V"), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(readout("AM1"), QStringLiteral("10mA"), 10000);
        // A selected wire shows its net's voltage among the live values.
        const model::Id wire = doc().schematic().wires().front().id;
        QVERIFY(window_->run_controller()->live_text({wire}).startsWith(QStringLiteral("V(")));
        QCOMPARE(window_->messages()->warning_count(), before);
        window_->stop();
#endif
    }

    void keypad_key_clicked_reaches_the_firmware()
    {
        // SN-210: example 30; holding the mouse on key "5" of the keypad, the Blue Pill reports it.
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/30_Keypad_4x4_scanned_by_Blue_Pill.snx"), errors, warnings));
        model::Id keypad = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            keypad = c.type == "keypad_4x4" ? c.id : keypad;
        }
        QVERIFY(keypad != model::no_id);
        const auto* definition = doc().library().find("keypad_4x4");
        QVERIFY(definition && definition->interactive && definition->interactive->keys.size() == 16);
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        auto uart_text = [&] {
            std::string text;
            for (const auto& port : window_->run_controller()->uart_ports()) {
                std::string bytes;
                window_->run_controller()->uart_received(port, 0, bytes);
                text += bytes;
            }
            return text;
        };
        QTRY_VERIFY_WITH_TIMEOUT(uart_text().find("Keypad ready") != std::string::npos, 20000);
        // The middle of key "5" (the sixth key) on screen.
        const auto& key = definition->interactive->keys[5];
        view()->centerOn(window_->scene()->item_of(keypad));
        const QPointF local((key[0] + key[2] / 2) * scene_grid, (key[1] + key[3] / 2) * scene_grid);
        const QPoint on_view = view()->mapFromScene(window_->scene()->item_of(keypad)->mapToScene(local));
        QTest::mousePress(view()->viewport(), Qt::LeftButton, Qt::NoModifier, on_view);
        QTRY_COMPARE(window_->scene()->live().controls.at(keypad), 6.0);
        QTRY_VERIFY_WITH_TIMEOUT(uart_text().find("Key: 5") != std::string::npos, 20000);
        QTest::mouseRelease(view()->viewport(), Qt::LeftButton, Qt::NoModifier, on_view);
        QTRY_COMPARE(window_->scene()->live().controls.at(keypad), 0.0);
        window_->stop();
#endif
    }

    void converters_quantize_while_running()
    {
        // SN-211: example 31; the 8-bit ADC and DAC turn V3 into steps of 5 V / 256.
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/31_AD_and_DA_converters.snx"), errors, warnings));
        std::map<std::string, model::Id> ids;
        for (const auto& c : doc().schematic().components()) {
            ids[c.designator] = c.id;
        }
        const int before = window_->messages()->warning_count();
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        auto net = [&](const char* name) {
            const auto& nets = window_->scene()->live().nets;
            const auto it = nets.find(name);
            return it == nets.end() ? -1.0 : it->second;
        };
        // V3 at 50 %: code 128 of 256, 2.5 V; at 30 %: 1.5 V gives code 76, 1.484 V.
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->status().sim_time > 0.05, 10000);
        window_->run_controller()->set_control(ids["V3"], 0.3);
        QTRY_VERIFY_WITH_TIMEOUT(window_->scene()->live().readouts.count(ids["VM1"])
                && window_->scene()->live().readouts.at(ids["VM1"]).startsWith(QStringLiteral("1.48")),
            10000);
        // The 4-bit staircase: OUT4 is always a multiple of 5 V / 16 (between the steps' 10 ns edges).
        QTRY_VERIFY_WITH_TIMEOUT(net("out4") > 0.1, 10000);
        const double out4 = net("out4") / (5.0 / 16);
        QVERIFY(std::abs(out4 - std::round(out4)) < 0.01);
        QCOMPARE(window_->messages()->warning_count(), before);
        window_->stop();
#endif
    }

    void rom_without_a_board_drives_a_display()
    {
        // SN-212: example 32 through the engine process; the Memory Viewer shows the ROM's file.
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/32_ROM_lookup_seven_segment.snx"), errors, warnings));
        model::Id rom = model::no_id;
        model::Id display = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            rom = c.type == "rom" ? c.id : rom;
            display = c.type == "seven_segment_cc" ? c.id : display;
        }
        QVERIFY(rom != model::no_id && display != model::no_id);
        const int before = window_->messages()->warning_count();
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        // Some segments lit, and the picture changes with the count.
        auto picture = [&] {
            const auto& lights = window_->scene()->live().lights;
            const auto it = lights.find(display);
            QString lit;
            if (it != lights.end()) {
                for (const double level : it->second.levels) {
                    lit += level > 0.3 ? QLatin1Char('1') : QLatin1Char('0');
                }
            }
            return lit;
        };
        QTRY_VERIFY_WITH_TIMEOUT(picture().contains(QLatin1Char('1')), 10000);
        const QString first = picture();
        QTRY_VERIFY_WITH_TIMEOUT(picture().contains(QLatin1Char('1')) && picture() != first, 10000);
        QCOMPARE(window_->messages()->warning_count(), before);
        window_->scene()->select_ids({rom});
        window_->memory_viewer()->show_chip(rom);
        const auto& bytes = window_->memory_viewer()->bytes();
        QCOMPARE(bytes.size(), std::size_t{16});
        QCOMPARE(bytes[0], std::uint8_t{0x3F});
        QCOMPARE(bytes[15], std::uint8_t{0x71});
        window_->memory_viewer()->close();
        window_->stop();
#endif
    }

    void timers_555_blink_and_time_a_pulse()
    {
        // SN-213: example 34; the astable keeps blinking across segments, the monostable times SW1.
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/34_Timer_555_astable_and_monostable.snx"), errors, warnings));
        std::map<std::string, model::Id> ids;
        for (const auto& c : doc().schematic().components()) {
            ids[c.designator] = c.id;
        }
        // In real time (the example's speed): what the canvas shows keeps up with the simulated time.
        const int before = window_->messages()->warning_count();
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        auto lit = [&](const char* led) {
            const auto& glow = window_->scene()->live().glow;
            const auto it = glow.find(ids[led]);
            return it != glow.end() && it->second.level > 0.3;
        };
        // About 1 Hz: count the LED's changes over 3.5 s of simulated time (three segments).
        int changes = 0;
        bool last = lit("LED1");
        QElapsedTimer wall;
        wall.start();
        while (window_->run_controller()->status().sim_time < 3.5 && wall.elapsed() < 60000) {
            QTest::qWait(10);
            if (lit("LED1") != last) {
                last = !last;
                ++changes;
            }
        }
        QVERIFY2(changes >= 5 && changes <= 8, qPrintable(QString::number(changes)));
        QVERIFY(!lit("LED2"));
        // A short press on SW1: the green LED lights for about 1.1 s.
        window_->run_controller()->set_control(ids["SW1"], 1.0);
        QTRY_VERIFY_WITH_TIMEOUT(lit("LED2"), 10000);
        const double on = window_->run_controller()->status().sim_time;
        window_->run_controller()->set_control(ids["SW1"], 0.0);
        QTRY_VERIFY_WITH_TIMEOUT(!lit("LED2"), 20000);
        const double length = window_->run_controller()->status().sim_time - on;
        QVERIFY2(length > 0.8 && length < 1.4, qPrintable(QString::number(length)));
        QCOMPARE(window_->messages()->warning_count(), before);
        window_->stop();
#endif
    }

    void vendor_subcircuit_is_placed_and_runs()
    {
        // SN-214: Place > Subcircuit takes a .subckt from the model folders as a part of its own.
        qputenv("SIMNODUS_MODELS_DIR", QByteArray(SIMNODUS_TEST_EXAMPLES_DIR "/models"));
        QVERIFY(window_->begin_subcircuit("DARLINGTON4"));
        QTest::mouseClick(view()->viewport(), Qt::LeftButton, Qt::NoModifier, at({10, 10}));
        qunsetenv("SIMNODUS_MODELS_DIR");
        model::Id placed = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            placed = c.type == "subckt:DARLINGTON4" ? c.id : placed;
        }
        QVERIFY(placed != model::no_id);
        const auto* c = doc().schematic().component(placed);
        QVERIFY(c->models.count("model") && c->models.at("model").find(".subckt DARLINGTON4") != std::string::npos);
        const auto* d = doc().library().find("subckt:DARLINGTON4");
        QVERIFY(d && d->pins.size() == 10);
#if defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        // Example 35 keeps its subcircuit: it runs without the model folder.
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/35_Vendor_subcircuit_Darlington_driver.snx"), errors, warnings));
        std::map<std::string, model::Id> ids;
        for (const auto& part : doc().schematic().components()) {
            ids[part.designator] = part.id;
        }
        const int before = window_->messages()->warning_count();
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        auto lit = [&](const char* led) {
            const auto& glow = window_->scene()->live().glow;
            const auto it = glow.find(ids[led]);
            return it != glow.end() && it->second.level > 0.3;
        };
        QTRY_VERIFY_WITH_TIMEOUT(lit("LED1"), 10000); // SW1 closed
        QVERIFY(!lit("LED2"));
        window_->run_controller()->set_control(ids["SW2"], 1.0);
        QTRY_VERIFY_WITH_TIMEOUT(lit("LED2"), 10000);
        QCOMPARE(window_->messages()->warning_count(), before);
        window_->stop();
#endif
    }

    void memory_viewer_shows_the_eeprom_while_running()
    {
        // SN-208: View... on the 24C02 opens the Memory Viewer; the firmware's write appears live.
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(
            QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/27_I2C_EEPROM_sensor_expander.snx"), errors, warnings));
        model::Id eeprom = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            eeprom = c.type == "24c02" ? c.id : eeprom;
        }
        QVERIFY(eeprom != model::no_id);
        window_->scene()->select_ids({eeprom});
        QPushButton* view = nullptr;
        QTRY_VERIFY((view = window_->properties()->findChild<QPushButton*>(QStringLiteral("view_contents"))) != nullptr);
        view->click();
        auto* viewer = window_->memory_viewer();
        QVERIFY(viewer->isVisible());
        auto* chips = viewer->findChild<QComboBox*>(QStringLiteral("memoryChips"));
        QCOMPARE(chips->count(), 1);
        QVERIFY(chips->currentText().startsWith(QStringLiteral("U2 (")));
        // Not run yet: erased.
        QCOMPARE(viewer->bytes().size(), std::size_t{256});
        QCOMPARE(viewer->bytes()[0x10], std::uint8_t{0xFF});
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        auto written = [&] {
            const auto& b = viewer->bytes();
            return b.size() == 256 && std::string(b.begin() + 0x10, b.begin() + 0x18) == "SimNodus";
        };
        QTRY_VERIFY_WITH_TIMEOUT(written(), 20000);
        auto* table = viewer->findChild<QTableWidget*>(QStringLiteral("memoryTable"));
        QCOMPARE(table->item(1, 0)->text(), QStringLiteral("53")); // 'S'
        QCOMPARE(table->item(1, 16)->text().left(8), QStringLiteral("SimNodus"));
        // What Load... does while running, through the engine process.
        std::vector<std::uint8_t> zeros(256, 0x00);
        std::copy_n("SimNodus", 8, zeros.begin() + 0x10);
        window_->run_controller()->load_device_memory(eeprom, zeros);
        QTRY_VERIFY_WITH_TIMEOUT(viewer->bytes()[0] == 0x00, 5000);
        QVERIFY(written());
        window_->stop();
        QTRY_VERIFY_WITH_TIMEOUT(!window_->run_controller()->active(), 5000);
        QTest::qWait(300);
        QVERIFY(written()); // the last run's contents stay on display
        viewer->close();
#endif
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

    void analyzer_stays_responsive_with_a_long_timebase()
    {
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        // 1 s/div shows 10 s: up to 100 000 samples per channel at the 100 us step.
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/02_RC_filter_with_pulse.snx"), errors, warnings));
        auto settings = doc().schematic().settings();
        settings.speed = 0; // as fast as possible
        doc().apply(editing::set_settings(doc().schematic(), settings));
        window_->show_analyzer();
        AnalyzerWindow* analyzer = window_->analyzer();
        analyzer->timebase()->setCurrentIndex(analyzer->timebase()->findData(1.0));
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->status().sim_time > 12.0, 120000);
        const auto& channels = window_->run_controller()->channels();
        QVERIFY(!channels.empty());
        const auto held = window_->run_controller()->samples(channels.front(), 0.0).size();
        qInfo("%.1f s simulated, %zu samples held per channel", window_->run_controller()->status().sim_time, held);
        QVERIFY(held > 90000);
        for (const int tab : {0, 1}) {
            analyzer->tabs()->setCurrentIndex(tab);
            QElapsedTimer timer;
            timer.start();
            for (int i = 0; i < 20; ++i) {
                analyzer->refresh();
                analyzer->repaint(); // drawing included
            }
            const double each = static_cast<double>(timer.elapsed()) / 20.0;
            QElapsedTimer paint_only;
            paint_only.start();
            for (int i = 0; i < 20; ++i) {
                analyzer->scope()->repaint();
            }
            qInfo("  scope repaint alone: %.1f ms", static_cast<double>(paint_only.elapsed()) / 20.0);
            paint_only.restart();
            for (int i = 0; i < 20; ++i) {
                analyzer->repaint();
            }
            qInfo("  window repaint alone: %.1f ms", static_cast<double>(paint_only.elapsed()) / 20.0);
            qInfo("analyzer refresh at 1 s/div, %s tab: %.1f ms", tab == 0 ? "Scope" : "Logic", each);
            QVERIFY(each < 80.0); // 16 ms on the owner's laptop, 130 ms before the fix
        }
        window_->stop();
#endif
    }

    void scrolling_traces_do_not_tremble()
    {
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        // Owner's report of 2026-10-19: example 03 at 1 s/div, the potentiometer moving.
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/03_Potentiometer_and_meters.snx"), errors, warnings));
        model::Id pot = model::no_id;
        for (const auto& c : doc().schematic().components()) {
            if (c.type == "potentiometer") {
                pot = c.id;
            }
        }
        window_->show_analyzer();
        AnalyzerWindow* analyzer = window_->analyzer();
        analyzer->timebase()->setCurrentIndex(analyzer->timebase()->findData(1.0));
        window_->run();
        QElapsedTimer wall;
        wall.start();
        int k = 0;
        // Past 10 s the window scrolls.
        while (window_->run_controller()->status().sim_time < 11.5 && wall.elapsed() < 60000) {
            QTest::qWait(40);
            window_->run_controller()->set_control(pot, 0.5 + 0.4 * std::sin(k++ * 0.2));
        }
        analyzer->refresh();
        const auto first = analyzer->scope()->traces();
        const double first_start = analyzer->scope()->window_start();
        QTest::qWait(300);
        analyzer->refresh();
        const auto second = analyzer->scope()->traces();
        const double start = analyzer->scope()->window_start();
        const double end = analyzer->scope()->window_end();
        QVERIFY(start > first_start); // it scrolled
        // The window starts on a pixel boundary of the plot.
        const double pixel = (end - start) / analyzer->scope()->plot_rect().width();
        QVERIFY(std::abs(start / pixel - std::round(start / pixel)) < 1e-6);
        // The stretch both frames show was thinned the same way.
        QVERIFY(!first.empty() && !second.empty());
        std::map<double, double> before;
        for (const auto& s : first[0].samples) {
            before[s.time] = s.value;
        }
        int compared = 0;
        int different = 0;
        for (const auto& s : second[0].samples) {
            if (s.time < start + 0.1 || s.time > first[0].samples.back().time - 0.1) {
                continue;
            }
            ++compared;
            const auto it = before.find(s.time);
            different += it == before.end() || it->second != s.value;
        }
        qInfo("%d samples compared, %d different", compared, different);
        QVERIFY(compared > 100);
        QCOMPARE(different, 0);
        window_->stop();
#endif
    }

    void trigger_point_is_marked_and_auto_runs_free_without_trigger()
    {
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        // Owner's report of 2026-10-19: with long timebases the triggered display
        // stood still on an old frame and showed no trigger point.
        auto run_triggered = [&](const char* example, double level) {
            QStringList errors;
            QStringList warnings;
            QVERIFY(doc().open_copy(QString::fromUtf8(SIMNODUS_TEST_EXAMPLES_DIR) + QLatin1Char('/') + QLatin1String(example),
                errors, warnings));
            auto settings = doc().schematic().settings();
            settings.speed = 0;
            doc().apply(editing::set_settings(doc().schematic(), settings));
            window_->show_analyzer();
            AnalyzerWindow* analyzer = window_->analyzer();
            analyzer->timebase()->setCurrentIndex(analyzer->timebase()->findData(0.1)); // 100 ms/div
            analyzer->findChild<QComboBox*>(QStringLiteral("scopeMode"))->setCurrentIndex(1);
            const auto level_box = analyzer->findChildren<QDoubleSpinBox*>();
            for (auto* box : level_box) {
                if (box->value() == 1.0 && box->decimals() == 4) {
                    box->setValue(level);
                }
            }
            window_->run();
            QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->status().sim_time > 4.0, 60000);
            analyzer->refresh();
        };
        // A 2 Hz pulse crosses 2.5 V twice a period: triggered frames with the T marker.
        run_triggered("02_RC_filter_with_pulse.snx", 2.5);
        auto* scope = window_->analyzer()->scope();
        QVERIFY(scope->trigger_point().has_value());
        QVERIFY(*scope->trigger_point() >= scope->window_start() && *scope->trigger_point() <= scope->window_end());
        QVERIFY(scope->trigger_state().startsWith(QStringLiteral("Triggered at")));
        // It stays triggered: new crossings keep arriving (no alternation with free running).
        for (int i = 0; i < 10; ++i) {
            QTest::qWait(200);
            window_->analyzer()->refresh();
            QVERIFY2(scope->trigger_state().startsWith(QStringLiteral("Triggered at")), qPrintable(scope->trigger_state()));
        }
        window_->stop();
        // Steady voltages never cross 100 V: Auto runs free instead of standing still.
        run_triggered("03_Potentiometer_and_meters.snx", 100.0);
        QVERIFY(!scope->trigger_point().has_value());
        QVERIFY(scope->trigger_state().contains(QStringLiteral("running free")));
        // It moves on (a slow CI runner may need more than one display refresh).
        const double end = scope->window_end();
        QTRY_VERIFY_WITH_TIMEOUT((window_->analyzer()->refresh(), scope->window_end() > end), 5000);
        window_->stop();
#endif
    }

    void pause_button_shows_the_pause_and_live_values_scroll()
    {
        // Live values of a board (many pins) scroll inside the panel.
        auto* live = window_->properties()->findChild<QScrollArea*>(QStringLiteral("liveValuesScroll"));
        QVERIFY(live);
        QVERIFY(live->widget() && live->widget()->objectName() == QStringLiteral("liveValues"));
#if !defined(SIMNODUS_TEST_NGSPICE_LIBRARY)
        QSKIP("No ngspice configured for this build.");
#else
        QStringList errors;
        QStringList warnings;
        QVERIFY(doc().open_copy(QStringLiteral(SIMNODUS_TEST_EXAMPLES_DIR "/03_Potentiometer_and_meters.snx"), errors, warnings));
        auto* pause = window_->findChild<QAction*>(QStringLiteral("pauseAction"));
        QVERIFY(pause && pause->isCheckable());
        window_->run();
        QTRY_VERIFY(window_->run_controller()->state() == sim::RunState::Running);
        QVERIFY(!pause->isChecked());
        pause->trigger();
        QTRY_VERIFY(window_->run_controller()->state() == sim::RunState::Paused);
        QTRY_VERIFY(pause->isChecked()); // pressed while paused
        pause->trigger();
        QTRY_VERIFY(window_->run_controller()->state() == sim::RunState::Running);
        QTRY_VERIFY(!pause->isChecked());
        window_->stop();
        QVERIFY(!pause->isChecked());
#endif
    }

    void dark_theme_applies_at_once_and_is_remembered()
    {
        // View > Theme (SN-162): the colors change while the window is open.
        auto* dark = window_->findChild<QAction*>(QStringLiteral("theme_dark"));
        auto* light = window_->findChild<QAction*>(QStringLiteral("theme_light"));
        auto* system = window_->findChild<QAction*>(QStringLiteral("theme_system"));
        QVERIFY(dark && light && system);
        dark->trigger();
        QVERIFY(theme::dark());
        QCOMPARE(settings_->value(QStringLiteral("theme")).toString(), QStringLiteral("dark"));
        QVERIFY(theme::canvas_background.lightness() < 60);
        QVERIFY(theme::ink.lightness() > 180);
        QVERIFY(qApp->palette().color(QPalette::Window).lightness() < 80);
        // Drawing still works with the dark colors.
        place("resistor", {0, 0});
        QVERIFY(!view()->grab().isNull());
        light->trigger();
        QVERIFY(!theme::dark());
        QVERIFY(theme::canvas_background.lightness() > 200);
        QCOMPARE(settings_->value(QStringLiteral("theme")).toString(), QStringLiteral("light"));
        system->trigger();
        QVERIFY(!settings_->contains(QStringLiteral("theme")));
        theme::apply(*qApp); // the other tests expect the light colors
    }

    void language_choice_is_saved_and_the_translation_is_complete()
    {
        // Tools > Language keeps the choice for the next start (UI-16).
        QAction* portuguese = nullptr;
        QAction* system = nullptr;
        for (auto* action : window_->findChildren<QAction*>()) {
            if (action->isCheckable() && action->data().toString() == QStringLiteral("pt_BR")) {
                portuguese = action;
            }
            if (action->isCheckable() && action->data().isValid() && action->data().toString().isEmpty()
                && action->text().contains(QStringLiteral("System"))) {
                system = action;
            }
        }
        QVERIFY(portuguese && system);
        portuguese->trigger();
        QCOMPARE(settings_->value(QStringLiteral("language")).toString(), QStringLiteral("pt_BR"));
        system->trigger();
        QVERIFY(!settings_->contains(QStringLiteral("language")));
#ifdef SIMNODUS_TEST_TRANSLATIONS_DIR
        // The built translation loads and translates the interface and the library.
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(SIMNODUS_TEST_TRANSLATIONS_DIR "/simnodus_pt_BR.qm")));
        QCOMPARE(translator.translate("MainWindow", "&File"), QStringLiteral("&Arquivo"));
        QCOMPARE(translator.translate("ComponentLibrary", "Resistor"), QStringLiteral("Resistor"));
        QCOMPARE(translator.translate("ComponentLibrary", "Potentiometer"), QStringLiteral("PotenciÃ´metro"));
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
        analyzer->tabs()->setCurrentIndex(1); // the lanes are read while the Logic tab is shown
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
        // Below the lanes: only the cursors, their distance (period) and its inverse (frequency).
        QVERIFY(analyzer->transitions()->text().contains(QStringLiteral("Cursors")));
        analyzer->scope()->set_cursors_enabled(true);
        analyzer->scope()->set_cursor(0, 1.0);
        analyzer->scope()->set_cursor(1, 1.5);
        QTRY_VERIFY(analyzer->transitions()->text().contains(QStringLiteral("period")));
        QVERIFY(analyzer->transitions()->text().contains(QStringLiteral("(frequency) 2Hz")));
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

    void external_control_edits_the_document_with_undo()
    {
        const auto hello = control("hello");
        QVERIFY(hello.is_object());
        QCOMPARE(hello["window"].get<bool>(), true);
        control_rc();
        QCOMPARE(doc().schematic().components().size(), std::size_t{4});
        QCOMPARE(doc().undo_stack()->count(), 8); // one step per command
        QVERIFY(window_->isWindowModified());
        // The last change is selected on the canvas.
        const auto* r1 = doc().schematic().component_by_designator("R1");
        QVERIFY(control("component.set", {{"component", "R1"}, {"parameters", {{"R", "2k"}}}}).is_object());
        QCOMPARE(window_->scene()->selected_ids(), (std::set<model::Id>{r1->id}));
        QCOMPARE(doc().schematic().component(r1->id)->parameters.at("R"), std::string("2k"));
        QCOMPARE(control("edit.undo")["undone"].get<bool>(), true);
        QCOMPARE(doc().schematic().component(r1->id)->parameters.at("R"), std::string("1k"));
        // Unsaved changes are not dropped unless asked.
        QVERIFY(control("project.new").is_null());
        QCOMPARE(last_reply_["error"]["code"].get<std::string>(), std::string("blocked"));
        QCOMPARE(doc().schematic().components().size(), std::size_t{4});
        const QString file = dir_.filePath(QStringLiteral("controlled.snx"));
        QVERIFY(control("project.save", {{"path", file.toStdString()}}).is_object());
        QVERIFY(!window_->isWindowModified());
        QCOMPARE(doc().file_path(), file);
        QVERIFY(control("project.new").is_object());
        QVERIFY(doc().schematic().components().empty());
        QVERIFY(control("project.open", {{"path", file.toStdString()}}).is_object());
        QCOMPARE(doc().schematic().components().size(), std::size_t{4});
        // The window's own commands.
        QVERIFY(control("view.caption", {{"text", "Step 1: the source"}})["shown"].get<bool>());
        auto* caption = view()->viewport()->findChild<QLabel*>(QStringLiteral("controlCaption"));
        QVERIFY(caption && caption->isVisible());
        QCOMPARE(caption->text(), QStringLiteral("Step 1: the source"));
        QVERIFY(control("view.caption", {{"text", ""}}).is_object());
        QVERIFY(!caption->isVisible());
        QCOMPARE(control("view.pace", {{"ms", 50}})["ms"].get<int>(), 50);
        QElapsedTimer paced;
        paced.start();
        QVERIFY(control("items.move", {{"items", {"C1"}}, {"by", {2, 0}}}).is_object());
        QVERIFY(paced.elapsed() >= 40);
        QVERIFY(control("view.show", {{"items", {"V1"}}})["selected"].get<int>() == 1);
        QVERIFY(control("view.show", {{"items", {"X9"}}}).is_null());
        // A grid area framed in the left half of the canvas, before anything is placed there.
        QVERIFY(control("view.fit", {{"area", {40, 40, 60, 50}}, {"region", {0, 0, 0.5, 1}}})["zoom"].get<double>() > 0);
        const QRect canvas = view()->viewport()->rect();
        const QPoint middle = view()->mapFromScene(QPointF(50 * SchematicView::grid, 45 * SchematicView::grid));
        QVERIFY2(std::abs(middle.x() - canvas.width() / 4) <= 2, qPrintable(QString::number(middle.x())));
        QVERIFY(std::abs(middle.y() - canvas.height() / 2) <= 2);
        QVERIFY(control("view.fit", {{"region", {0.6, 0, 0.6, 1}}}).is_null());
        QCOMPARE(last_reply_["error"]["code"].get<std::string>(), std::string("invalid-argument"));
        QVERIFY(control("view.fit").is_object());
        const auto help = control("help");
        QVERIFY(std::any_of(help["commands"].begin(), help["commands"].end(),
            [](const automation::json& c) { return c["command"] == "view.caption"; }));
    }

    void external_control_shows_the_hand()
    {
        // SN-238: captions and key badges placed, a drawn pointer, outlines,
        // screen positions, and edits through the real widgets.
        control_rc();
        QVERIFY(control("view.caption", {{"text", "Top"}, {"position", "top"}, {"size", 2}})["shown"].get<bool>());
        auto* caption = view()->viewport()->findChild<QLabel*>(QStringLiteral("controlCaption"));
        QVERIFY(caption && caption->y() < view()->viewport()->height() / 3);
        QVERIFY(control("view.caption", {{"text", "x"}, {"position", "left"}}).is_null());
        QCOMPARE(last_reply_["error"]["code"].get<std::string>(), std::string("invalid-argument"));
        QVERIFY(control("view.keys", {{"keys", "Ctrl+R"}, {"position", "bottom-right"}})["shown"].get<bool>());
        auto* keys = view()->viewport()->findChild<QLabel*>(QStringLiteral("controlKeys"));
        QVERIFY(keys && keys->isVisible());
        QCOMPARE(keys->text(), QStringLiteral("Ctrl+R"));
        QVERIFY(keys->x() > view()->viewport()->width() / 2);

        auto state = control("view.pointer", {{"to", "R1.2"}, {"ms", 50}, {"click", 1}});
        QVERIFY2(state.is_object(), last_reply_.dump().c_str());
        QVERIFY(state["shown"].get<bool>());
        const auto located = control("view.locate", {{"targets", {"R1.2", "R1", {{"action", "Wire"}}, {{"part", "Resistor"}},
                                                                    {{"widget", "components"}}}}});
        QVERIFY2(located.is_object(), last_reply_.dump().c_str());
        QCOMPARE(located["targets"].size(), std::size_t{5});
        QVERIFY(state["point"] == located["targets"][0]["point"]);
        const auto& wire_button = located["targets"][2];
        const double scale = wire_button["scale"].get<double>();
        QVERIFY(std::abs(wire_button["physical_rect"][2].get<double>() - wire_button["rect"][2].get<double>() * scale) <= 1);
        QVERIFY(control("view.locate", {{"targets", {{{"widget", "nowhere"}}}}}).is_null());
        QVERIFY(control("view.highlight", {{"target", {{"widget", "components"}}}, {"seconds", 0}})["shown"].get<bool>());
        QVERIFY(control("view.highlight", {{"hide", true}}).is_object());
        QVERIFY(!control("view.pointer", {{"hide", true}})["shown"].get<bool>());

        // A resistor placed by hand: the R key, the part follows the pointer, a click drops it.
        const auto parts = doc().schematic().components().size();
        auto pressed = control("ui.key", {{"keys", "R"}});
        QVERIFY2(pressed.is_object(), last_reply_.dump().c_str());
        QCOMPARE(pressed["action"].get<std::string>(), std::string("Resistor"));
        QVERIFY(control("ui.move", {{"to", {30, 10}}, {"ms", 60}}).is_object());
        QVERIFY(control("ui.click", {{"target", {30, 10}}, {"ms", 30}}).is_object());
        QTRY_COMPARE(doc().schematic().components().size(), parts + 1);
        QVERIFY(control("ui.key", {{"keys", "Escape"}, {"show", false}}).is_object());
        // A wire from pin to pin with the Wire button.
        const auto wires = doc().schematic().wires().size();
        QVERIFY(control("ui.click", {{"target", {{"action", "Wire"}}}, {"ms", 30}}).is_object());
        QTRY_COMPARE(view()->tool(), Tool::Wire);
        const auto& placed = doc().schematic().components().back();
        QVERIFY(control("ui.click", {{"target", placed.designator + ".2"}, {"ms", 30}}).is_object());
        QVERIFY(control("ui.click", {{"target", "C1.1"}, {"ms", 30}}).is_object());
        QTRY_COMPARE(doc().schematic().wires().size(), wires + 1);
        QVERIFY(control("ui.key", {{"keys", "Escape"}, {"show", false}}).is_object());
        // Typing into the Components search filters the tree; a double-click on an entry starts placing it.
        QVERIFY(control("ui.type", {{"target", {{"widget", "search"}}}, {"text", "capac"}, {"ms", 5}}).is_object());
        QCOMPARE(window_->components()->search_field()->text(), QStringLiteral("capac"));
        QVERIFY(control("ui.click", {{"target", {{"part", "Capacitor"}}}, {"count", 2}, {"ms", 30}}).is_object());
        QTRY_VERIFY(view()->tool() != Tool::Select);
        QVERIFY(control("ui.key", {{"keys", "Escape"}, {"show", false}}).is_object());
        // A value typed into Properties and Enter.
        const auto* r1 = doc().schematic().component_by_designator("R1");
        window_->scene()->select_ids({r1->id});
        QTRY_VERIFY(window_->properties()->findChild<QLineEdit*>(QStringLiteral("parameter_R")));
        const auto typed = control("ui.type", {{"target", {{"field", "Resistance"}}}, {"text", "2k2"}, {"ms", 5}, {"enter", true}});
        QVERIFY2(typed.is_object(), last_reply_.dump().c_str());
        QTRY_COMPARE(doc().schematic().component(r1->id)->parameters.at("R"), std::string("2k2"));
        QVERIFY(control("ui.click", {{"target", {{"field", "Nothing"}}}}).is_null());
        QCOMPARE(last_reply_["error"]["code"].get<std::string>(), std::string("not-found"));
        QVERIFY(control("ui.key", {{"keys", ""}}).is_null());
        control("view.pointer", {{"hide", true}});
        control("view.caption", {{"text", ""}, {"position", "bottom"}});
    }

    void external_control_runs_show_in_the_window()
    {
        if (!engine_available()) {
            QSKIP("ngspice not available");
        }
        control_rc();
        const auto run = control("sim.run",
            {{"mode", "interactive"}, {"duration", "40m"}, {"speed", 0}, {"signals", {"V(OUT)"}}, {"keep", true}});
        QVERIFY2(run.is_object(), last_reply_.dump().c_str());
        QVERIFY(std::abs(run["signals"][0]["frequency"].get<double>() - 100.0) < 2.0);
        // The window's run controller ran it, so the canvas and the analyzer have it.
        QVERIFY(window_->run_controller()->active());
        QCOMPARE(window_->run_controller()->state(), sim::RunState::Paused);
        QVERIFY(window_->run_controller()->status().sim_time >= 0.04);
        QVERIFY(control("sim.read").is_object());
        // A run the user starts replaces the agent's.
        window_->stop();
        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(window_->run_controller()->active(), 5000);
        QVERIFY(control("sim.read").is_null());
        QCOMPARE(last_reply_["error"]["code"].get<std::string>(), std::string("not-running"));
        window_->stop();
        // A change of the circuit's structure stops a run, as in the window.
        QVERIFY(control("sim.start", {{"signals", {"V(OUT)"}}}).is_object());
        QVERIFY(window_->run_controller()->active());
        QVERIFY(control("component.add", {{"type", "resistor"}, {"at", {30, 0}}}).is_object());
        QVERIFY(!window_->run_controller()->active());
    }

    void external_control_sets_the_analyzer()
    {
        if (!engine_available()) {
            QSKIP("ngspice not available");
        }
        control_rc();
        QVERIFY(control("probe.add", {{"at", "C1.1"}}).is_object());
        QVERIFY(control("probe.add", {{"at", "V1.-"}}).is_object()); // ground: a channel without range
        QVERIFY(control("view.analyzer", {{"show", true}}).is_object());
        const auto run = control("sim.run",
            {{"mode", "interactive"}, {"duration", "40m"}, {"speed", 0}, {"signals", {"V(OUT)"}}, {"keep", true}});
        QVERIFY2(run.is_object(), last_reply_.dump().c_str());
        auto* analyzer = window_->analyzer();
        QTRY_VERIFY(analyzer->isVisible());
        auto state = control("analyzer.set");
        QVERIFY2(state.is_object(), last_reply_.dump().c_str());
        QCOMPARE(state["channels"].size(), std::size_t{2});
        const std::string out = state["channels"][0]["name"].get<std::string>();
        const std::string flat = state["channels"][1]["name"].get<std::string>();

        // A channel at 0 V takes its unit's scale instead of a tiny one.
        state = control("analyzer.autoscale");
        QVERIFY(state["channels"][1]["auto"].get<bool>());
        QCOMPARE(state["channels"][1]["scale"].get<double>(), state["channels"][0]["scale"].get<double>());

        state = control("analyzer.set",
            {{"tab", "logic"}, {"timebase", "2m"}, {"mode", "triggered"},
                {"trigger", {{"source", out}, {"level", "500m"}, {"slope", "falling"}, {"sweep", "normal"}}},
                {"channels", {{out, {{"scale", 0.5}, {"offset", 1}}}, {flat, {{"visible", false}}}}},
                {"select", flat}, {"lanes", {out}}, {"threshold", 0.4}, {"panel", false}});
        QVERIFY2(state.is_object(), last_reply_.dump().c_str());
        QCOMPARE(analyzer->tabs()->currentIndex(), 1);
        QCOMPARE(state["timebase"].get<double>(), 2e-3);
        QCOMPARE(state["mode"].get<std::string>(), std::string("triggered"));
        QCOMPARE(state["trigger"]["source"].get<std::string>(), out);
        QCOMPARE(state["trigger"]["level"].get<double>(), 0.5);
        QCOMPARE(state["trigger"]["slope"].get<std::string>(), std::string("falling"));
        QCOMPARE(state["trigger"]["sweep"].get<std::string>(), std::string("normal"));
        QCOMPARE(state["channels"][0]["scale"].get<double>(), 0.5);
        QVERIFY(!state["channels"][0]["auto"].get<bool>());
        QCOMPARE(state["channels"][0]["offset"].get<double>(), 1.0);
        QVERIFY(!state["channels"][1]["visible"].get<bool>());
        QCOMPARE(state["select"].get<std::string>(), flat);
        QVERIFY(state["lanes"] == automation::json::array({out})); // a probe in the Logic tab without a click
        QCOMPARE(state["threshold"].get<double>(), 0.4);
        QVERIFY(!state["panel"].get<bool>());
        QVERIFY(analyzer->findChild<QWidget*>(QStringLiteral("analyzerControls"))->isHidden());
        // Wrong arguments change nothing.
        QVERIFY(control("analyzer.set", {{"tab", "scope"}, {"channels", {{"V(NOPE)", {{"visible", true}}}}}}).is_null());
        QCOMPARE(last_reply_["error"]["code"].get<std::string>(), std::string("not-found"));
        QCOMPARE(analyzer->tabs()->currentIndex(), 1);
        QVERIFY(control("analyzer.set", {{"mode", "sideways"}}).is_null());
        QCOMPARE(last_reply_["error"]["code"].get<std::string>(), std::string("invalid-argument"));

        // The run is paused: the time axis zooms (a command, the wheel on the scope and on the lanes).
        QVERIFY(state["navigable"].get<bool>());
        state = control("analyzer.set", {{"tab", "scope"}, {"window", {0.010, 0.012}}, {"cursors", {0.0105, 0.011}}});
        QVERIFY2(state.is_object(), last_reply_.dump().c_str());
        QCOMPARE(state["window"][0].get<double>(), 0.010);
        QCOMPARE(state["window"][1].get<double>(), 0.012);
        QVERIFY(state["cursors"]["on"].get<bool>());
        QCOMPARE(state["cursors"]["a"].get<double>(), 0.0105);
        auto* scope = analyzer->scope();
        const QPointF middle = scope->plot_rect().center();
        QWheelEvent in(middle, scope->mapToGlobal(middle), {}, {0, 120}, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(scope, &in);
        QVERIFY(scope->window_end() - scope->window_start() < 0.0019);
        QVERIFY(scope->window_start() > 0.010 && scope->window_end() < 0.012);
        analyzer->set_tab(1);
        auto* lanes = analyzer->logic();
        const double before = scope->window_end() - scope->window_start();
        const QPointF lane_point(lanes->width() * 0.6, 20);
        QWheelEvent out_event(lane_point, lanes->mapToGlobal(lane_point), {}, {0, -120}, Qt::NoButton, Qt::NoModifier,
            Qt::NoScrollPhase, false);
        QApplication::sendEvent(lanes, &out_event);
        QVERIFY(scope->window_end() - scope->window_start() > before * 1.1);

        // Autoscale gives every channel back its automatic scale; the panel and the lanes come back.
        state = control("analyzer.autoscale");
        QVERIFY(state["channels"][0]["auto"].get<bool>());
        state = control("analyzer.set", {{"panel", true}, {"lanes", "auto"}, {"display", "stop"}});
        QVERIFY(state["panel"].get<bool>());
        QCOMPARE(state["display"].get<std::string>(), std::string("stop"));
        window_->stop();
        analyzer->set_display_stopped(false);
        QVERIFY(control("analyzer.set", {{"window", {0, 1}}}).is_null());
        QCOMPARE(last_reply_["error"]["code"].get<std::string>(), std::string("blocked"));
        analyzer->hide();
    }

    void external_control_listens_on_a_local_endpoint()
    {
        const QString name = QStringLiteral("simnodus-test-%1").arg(QCoreApplication::applicationPid());
        QString error;
        QVERIFY2(window_->control()->listen(error, name), qPrintable(error));
        QVERIFY(window_->control()->listening());
        // A second window cannot take the same endpoint.
        ControlServer other(*window_);
        QVERIFY(!other.listen(error, name));
        QVERIFY(!error.isEmpty());

        QLocalSocket socket;
        socket.connectToServer(name);
        QVERIFY(socket.waitForConnected(2000));
        QTRY_COMPARE(window_->control()->clients(), 1);
        auto* status = window_->statusBar()->findChild<QLabel*>(QStringLiteral("controlStatus"));
        QVERIFY(status && status->isVisible());
        socket.write("{\"id\": 7, \"command\": \"component.add\", \"args\": {\"type\": \"resistor\", \"at\": [0, 0]}}\n"
                     "not json\n");
        QByteArray received;
        QTRY_VERIFY_WITH_TIMEOUT((received += socket.readAll()).count('\n') >= 2, 5000);
        const auto lines = received.split('\n');
        const auto first = automation::json::parse(lines[0].toStdString());
        QCOMPARE(first["id"].get<int>(), 7);
        QVERIFY(first["ok"].get<bool>());
        QCOMPARE(doc().schematic().components().size(), std::size_t{1});
        const auto second = automation::json::parse(lines[1].toStdString());
        QCOMPARE(second["error"]["code"].get<std::string>(), std::string("bad-request"));
        socket.disconnectFromServer();
        QTRY_COMPARE(window_->control()->clients(), 0);
        QVERIFY(window_->set_external_control(false));
        QVERIFY(!window_->control()->listening());
        QVERIFY(!status->isVisible());
    }

    void closing_the_window_cancels_a_waiting_command()
    {
        const QString name = QStringLiteral("simnodus-test-close-%1").arg(QCoreApplication::applicationPid());
        QString error;
        QVERIFY2(window_->control()->listen(error, name), qPrintable(error));
        QLocalSocket socket;
        socket.connectToServer(name);
        QVERIFY(socket.waitForConnected(2000));
        socket.write("{\"command\": \"view.wait\", \"args\": {\"ms\": 30000}}\n");
        // The wait runs in a nested event loop, so the window is closed from inside it.
        QElapsedTimer elapsed;
        elapsed.start();
        bool waiting = false;
        QTimer::singleShot(300, window_, [this, &waiting] {
            waiting = window_->control()->busy();
            window_->close();
        });
        QByteArray received;
        QTRY_VERIFY_WITH_TIMEOUT((received += socket.readAll()).contains('\n'), 5000);
        QVERIFY(waiting);
        QVERIFY2(received.contains("\"cancelled\""), received.constData());
        QTRY_VERIFY(!window_->isVisible());
        QVERIFY(elapsed.elapsed() < 5000);
        QVERIFY(!window_->control()->listening());
    }

    void external_io_link_exchanges_signals_with_a_plant()
    {
        if (!engine_available()) {
            QSKIP("ngspice not available");
        }
        // A plant: a server that answers every line with its inputs (sensors).
        QTcpServer plant;
        QVERIFY(plant.listen(QHostAddress::LocalHost));
        auto options = window_->io_link()->options();
        options.port = plant.serverPort();
        window_->io_link()->set_options(options);

        auto* action = window_->findChild<QAction*>(QStringLiteral("externalIoAction"));
        QVERIFY(action && !action->isVisible()); // only in projects with external I/O parts
        QVERIFY(control("component.add", {{"type", "io_bit_in"}, {"at", {0, 0}}, {"parameters", {{"address", "%IX0.10"}}}}).is_object());
        QVERIFY(control("component.add", {{"type", "io_bit_out"}, {"at", {10, 0}}, {"mirrored", true},
            {"parameters", {{"address", "%QX1.0"}}}}).is_object());
        QVERIFY(control("wire.connect", {{"from", "IO1.1"}, {"to", "IO2.1"}}).is_object());
        QVERIFY(control("component.add", {{"type", "io_word_in"}, {"at", {0, 10}}, {"parameters", {{"address", "%IW3"}}}}).is_object());
        QVERIFY(control("component.add", {{"type", "io_word_out"}, {"at", {10, 10}}, {"mirrored", true},
            {"parameters", {{"address", "%QW4"}}}}).is_object());
        QVERIFY(control("wire.connect", {{"from", "IO3.1"}, {"to", "IO4.1"}}).is_object());
        QVERIFY(control("component.add", {{"type", "ground"}, {"at", {0, 20}}}).is_object());
        QVERIFY(action->isVisible());

        window_->run();
        QTRY_VERIFY_WITH_TIMEOUT(plant.hasPendingConnections(), 5000);
        QTcpSocket* socket = plant.nextPendingConnection();
        QByteArray buffer;
        automation::json last;
        // Answers each line with the inputs and keeps the last command (not {}) received.
        auto exchange = [&] {
            buffer += socket->readAll();
            for (qsizetype end = buffer.indexOf('\n'); end >= 0; end = buffer.indexOf('\n')) {
                const auto message = automation::json::parse(buffer.left(end).toStdString());
                if (!message.empty()) {
                    last = message; // {} only asks for the inputs (while paused)
                }
                buffer.remove(0, end + 1);
                socket->write("{\"%IX0.10\": true, \"%IW3\": 500, \"%IX9.9\": false}\n");
            }
            return last.is_object() && last.value("%QX1.0", false) && last.value("%QW4", -1) == 500;
        };
        QTRY_VERIFY_WITH_TIMEOUT(exchange(), 5000);
        auto* status = window_->statusBar()->findChild<QLabel*>(QStringLiteral("ioStatus"));
        QVERIFY(status && status->isVisible());

        // Pausing sends the safe values (the plant keeps the last command); resuming sends the circuit's again.
        auto safe = [&] { return last.value("%QX1.0", true) == false && last.value("%QW4", -1) == 0; };
        window_->pause_resume();
        QTRY_VERIFY_WITH_TIMEOUT((exchange(), safe()), 5000);
        window_->pause_resume();
        QTRY_VERIFY_WITH_TIMEOUT(exchange(), 5000);
        window_->stop();
        QTRY_VERIFY_WITH_TIMEOUT((exchange(), safe()), 5000);
        QTRY_VERIFY(!status->isVisible());
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
