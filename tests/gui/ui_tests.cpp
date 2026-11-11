// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Offscreen GUI smoke tests of the application skeleton (SN-102).
#include "analyzer_window.hpp"
#include "app_settings.hpp"
#include "component_panel.hpp"
#include "main_window.hpp"
#include "memory_window.hpp"
#include "messages_panel.hpp"
#include "properties_panel.hpp"
#include "schematic_view.hpp"
#include "scope_widget.hpp"
#include "theme.hpp"
#include "tone_output.hpp"
#include "wifi_dialog.hpp"

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "editing/operations.hpp"
#include "library/component_library.hpp"
#endif

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QSplitter>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QSet>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>

using namespace simnodus::ui;

class UiTests : public QObject {
    Q_OBJECT

private:
    QTemporaryDir dir_;
    int counter_ = 0;

    std::unique_ptr<QSettings> fresh_settings()
    {
        return open_settings(dir_.filePath(QStringLiteral("settings-%1.ini").arg(++counter_)));
    }

    static int panel_width(MainWindow& window, Panel panel)
    {
        QSplitter* splitter = panel == Panel::Messages ? window.rows() : window.columns();
        QWidget* widget = panel == Panel::Components ? static_cast<QWidget*>(window.components())
            : panel == Panel::Properties             ? static_cast<QWidget*>(window.properties())
                                                     : static_cast<QWidget*>(window.messages());
        return splitter->sizes().value(splitter->indexOf(widget));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(dir_.isValid());
        theme::apply(*qApp);
    }

    void three_columns_and_collapsed_messages()
    {
        auto settings = fresh_settings();
        MainWindow window(*settings);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.columns()->count(), 3);
        QCOMPARE(window.columns()->widget(0), static_cast<QWidget*>(window.components()));
        QCOMPARE(window.columns()->widget(1), static_cast<QWidget*>(window.schematic()));
        QCOMPARE(window.columns()->widget(2), static_cast<QWidget*>(window.properties()));
        QVERIFY(window.panel_visible(Panel::Components));
        QVERIFY(window.panel_visible(Panel::Properties));
        QVERIFY(!window.panel_visible(Panel::Messages));
        QCOMPARE(window.properties()->header_text(), QStringLiteral("No selection"));
    }

    void side_panel_restores_its_width()
    {
        auto settings = fresh_settings();
        MainWindow window(*settings);
        window.resize(1280, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QList<int> sizes = window.columns()->sizes();
        const int total = sizes[0] + sizes[1] + sizes[2];
        window.columns()->setSizes({310, total - 310 - sizes[2], sizes[2]});
        QCoreApplication::processEvents();
        const int before = panel_width(window, Panel::Components);
        QVERIFY(std::abs(before - 310) <= 2);
        window.set_panel_visible(Panel::Components, false);
        QVERIFY(!window.panel_visible(Panel::Components));
        QCOMPARE(window.remembered_size(Panel::Components), before);
        window.set_panel_visible(Panel::Components, true);
        QCoreApplication::processEvents();
        QVERIFY(std::abs(panel_width(window, Panel::Components) - before) <= 2);
    }

    void focus_canvas_hides_and_restores_panels()
    {
        auto settings = fresh_settings();
        MainWindow window(*settings);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.set_panel_visible(Panel::Messages, true);
        window.set_panel_visible(Panel::Properties, false);
        window.set_focus_canvas(true);
        QVERIFY(window.focus_canvas_active());
        QVERIFY(!window.panel_visible(Panel::Components));
        QVERIFY(!window.panel_visible(Panel::Messages));
        window.set_focus_canvas(false);
        QVERIFY(window.panel_visible(Panel::Components));
        QVERIFY(!window.panel_visible(Panel::Properties));
        QVERIFY(window.panel_visible(Panel::Messages));
    }

    void settings_persist_between_sessions()
    {
        auto settings = fresh_settings();
        {
            MainWindow window(*settings);
            window.resize(1200, 760);
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            QList<int> sizes = window.columns()->sizes();
            const int total = sizes[0] + sizes[1] + sizes[2];
            window.columns()->setSizes({330, total - 330 - sizes[2], sizes[2]});
            window.set_panel_visible(Panel::Properties, false);
            window.show_analyzer();
            window.analyzer()->timebase()->setCurrentIndex(5);
            window.analyzer()->tabs()->setCurrentIndex(2);
            window.close();
        }
        MainWindow window(*settings);
        window.show();
        window.show_restored_windows();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(!window.panel_visible(Panel::Properties));
        QVERIFY(window.panel_visible(Panel::Components));
        QVERIFY(std::abs(window.remembered_size(Panel::Components) - 330) <= 2);
        QVERIFY(std::abs(panel_width(window, Panel::Components) - 330) <= 2);
        QVERIFY(window.analyzer()->isVisible());
        QCOMPARE(window.analyzer()->timebase()->currentIndex(), 5);
        QCOMPARE(window.analyzer()->tabs()->currentIndex(), 2);
        window.close();
    }

    void geometry_from_missing_monitor_is_moved_on_screen()
    {
        auto settings = fresh_settings();
        QByteArray saved;
        {
            MainWindow window(*settings);
            window.setGeometry(40000, 30000, 900, 600);
            saved = window.saveGeometry();
        }
        MainWindow window(*settings);
        restore_window_geometry(window, saved);
        QVERIFY(is_on_available_screen(window.frameGeometry()));
        QVERIFY(!is_on_available_screen(QRect(40000, 30000, 900, 600)));
    }

    void schematic_zoom_is_bounded_and_anchored()
    {
        SchematicView view;
        view.resize(800, 600);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const QPoint anchor(200, 150);
        const QPointF before = view.mapToScene(anchor);
        view.set_zoom(3.0, anchor);
        QVERIFY(std::abs(view.zoom() - 3.0) < 1e-9);
        const QPointF after = view.mapToScene(anchor);
        QVERIFY(std::abs(after.x() - before.x()) < 1.0 && std::abs(after.y() - before.y()) < 1.0);
        QVERIFY(view.minor_grid_visible());
        view.set_zoom(1000.0, anchor);
        QCOMPARE(view.zoom(), SchematicView::max_zoom);
        view.set_zoom(1e-6, anchor);
        QCOMPARE(view.zoom(), SchematicView::min_zoom);
        QVERIFY(!view.minor_grid_visible());
        view.fit_content();
        QVERIFY(view.zoom() > SchematicView::min_zoom && view.zoom() < SchematicView::max_zoom);
        // The background must paint at every zoom without issues.
        QVERIFY(!view.grab().isNull());
    }

    void component_search_filters_the_tree()
    {
        simnodus::library::ComponentLibrary library;
        library.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
        ComponentPanel panel(library);
        const int all = panel.visible_component_count();
        QCOMPARE(all, static_cast<int>(library.definitions().size())); // component set A and the Blue Pill
        QVERIFY(all >= 21);
        panel.search_field()->setText(QStringLiteral("LED"));
        const int filtered = panel.visible_component_count();
        QVERIFY(filtered >= 1 && filtered < all);
        QCOMPARE(panel.selected_type(), QStringLiteral("led"));
        QSignalSpy spy(&panel, &ComponentPanel::placement_requested);
        QTest::keyClick(panel.search_field(), Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QStringLiteral("led"));
        panel.search_field()->clear();
        QCOMPARE(panel.visible_component_count(), all);
        // SN-285: every category closed or open at once.
        auto* tree = panel.findChild<QTreeWidget*>(QStringLiteral("componentTree"));
        auto open_categories = [tree] {
            int open = 0;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                open += tree->topLevelItem(i)->isExpanded() ? 1 : 0;
            }
            return open;
        };
        panel.findChild<QToolButton*>(QStringLiteral("componentCollapseAll"))->click();
        QCOMPARE(open_categories(), 0);
        panel.findChild<QToolButton*>(QStringLiteral("componentExpandAll"))->click();
        QCOMPARE(open_categories(), tree->topLevelItemCount());
        QCOMPARE(tree->actions().size(), 2); // the same two in the context menu
    }

    void memory_model_shows_a_large_memory_at_once()
    {
        // SN-285: a 4 MB flash (W25Q32) is a model over its bytes; the window
        // froze for seconds while it made a table item for each of them.
        MemoryModel model;
        std::vector<std::uint8_t> bytes(4u * 1024 * 1024, 0xFF);
        bytes[0x10] = 'S';
        QElapsedTimer clock;
        clock.start();
        model.set_bytes(bytes, true);
        QCOMPARE(model.rowCount(), 262144);
        QCOMPARE(model.columnCount(), 17);
        QCOMPARE(model.index(1, 0).data().toString(), QStringLiteral("53"));
        QCOMPARE(model.index(1, 16).data().toString(), QStringLiteral("S..............."));
        QCOMPARE(model.headerData(262143, Qt::Vertical, Qt::DisplayRole).toString(), QStringLiteral("3FFFF0"));
        // A page program and a sector erase: the bytes that differ are marked,
        // and only their rows are painted again.
        QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);
        bytes[0x123456] = 0x42;
        std::fill(bytes.begin() + 0x200000, bytes.begin() + 0x201000, std::uint8_t{0x00});
        model.set_bytes(bytes, false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toModelIndex().row(), 0x123456 / 16);
        QCOMPARE(spy.first().at(1).toModelIndex().row(), 0x200FFF / 16);
        QVERIFY(model.recent(0x123456));
        QVERIFY(model.recent(0x200800));
        QVERIFY(!model.recent(0x123457));
        QVERIFY(model.index(0x123456 / 16, 6).data(Qt::BackgroundRole).isValid());
        QVERIFY(!model.index(0, 0).data(Qt::BackgroundRole).isValid());
        model.set_bytes(bytes, false); // the same bytes again: nothing to paint
        QCOMPARE(spy.count(), 1);
        QVERIFY(clock.elapsed() < 1000);
        QTest::qWait(1600); // the marks fade
        QVERIFY(!model.expire());
        QVERIFY(!model.recent(0x123456));

        // SN-289: what the search field means, and where it is found.
        using Bytes = std::vector<std::uint8_t>;
        QVERIFY(MemoryModel::search_pattern(QStringLiteral("53 69 6d")) == (Bytes{0x53, 0x69, 0x6D}));
        QVERIFY(MemoryModel::search_pattern(QStringLiteral("5369")) == (Bytes{0x53, 0x69}));
        QVERIFY(MemoryModel::search_pattern(QStringLiteral("Sim")) == (Bytes{'S', 'i', 'm'}));
        QVERIFY(MemoryModel::search_pattern(QStringLiteral("536")) == (Bytes{'5', '3', '6'})); // not pairs: text
        QVERIFY(MemoryModel::search_pattern(QString()).empty());
        QCOMPARE(model.find(Bytes{0x42}, 0), qint64{0x123456});
        QCOMPARE(model.find(Bytes{0x42}, 0x123457), qint64{0x123456}); // round to the start
        QCOMPARE(model.find(Bytes{0x00, 0x00, 0xFF}, 0), qint64{0x200FFE});
        QCOMPARE(model.find(Bytes{0x12, 0x34}, 0), qint64{-1});
        QCOMPARE(model.find(Bytes{}, 0), qint64{-1});
        QCOMPARE(model.index_of(0x123456).row(), 0x12345);
        QCOMPARE(model.index_of(0x123456).column(), 6);

        // Editing: only when allowed, only byte cells, one or two hex digits; the
        // model reports the edit and leaves its bytes to the chip.
        QSignalSpy edits(&model, &MemoryModel::byte_edited);
        const QModelIndex cell = model.index(1, 0);
        QVERIFY(!(model.flags(cell) & Qt::ItemIsEditable));
        QVERIFY(!model.setData(cell, QStringLiteral("41")));
        model.set_editable(true);
        QVERIFY(model.flags(cell) & Qt::ItemIsEditable);
        QVERIFY(!(model.flags(model.index(1, 16)) & Qt::ItemIsEditable)); // the text column
        QCOMPARE(cell.data(Qt::EditRole).toString(), QStringLiteral("53"));
        QVERIFY(!model.setData(cell, QStringLiteral("1G")));
        QVERIFY(!model.setData(cell, QStringLiteral("123")));
        QVERIFY(!model.setData(cell, QString()));
        QCOMPARE(edits.count(), 0);
        QVERIFY(model.setData(cell, QStringLiteral(" a ")));
        QCOMPARE(edits.count(), 1);
        QCOMPARE(edits.first().at(0).toULongLong(), 0x10ULL);
        QCOMPARE(edits.first().at(1).toUInt(), 0x0Au);
        QCOMPARE(model.bytes()[0x10], std::uint8_t{'S'});
    }

    void analyzer_close_only_hides()
    {
        auto settings = fresh_settings();
        MainWindow window(*settings);
        window.show();
        window.show_analyzer();
        AnalyzerWindow* analyzer = window.analyzer();
        QVERIFY(QTest::qWaitForWindowExposed(analyzer));
        QVERIFY(!analyzer->testAttribute(Qt::WA_QuitOnClose));
        analyzer->close();
        QVERIFY(!analyzer->isVisible());
        QCOMPARE(window.analyzer(), analyzer);
        QCOMPARE(analyzer->tabs()->count(), 3);
        analyzer->timebase()->setCurrentIndex(0);
        QVERIFY(std::abs(analyzer->scope()->seconds_per_division() - 1e-6) < 1e-12);
    }

    void errors_open_the_messages_panel()
    {
        auto settings = fresh_settings();
        MainWindow window(*settings);
        window.show();
        QVERIFY(!window.panel_visible(Panel::Messages));
        window.messages()->add(Severity::Warning, QStringLiteral("a warning"));
        QVERIFY(!window.panel_visible(Panel::Messages));
        window.messages()->add(Severity::Error, QStringLiteral("an error"));
        QVERIFY(window.panel_visible(Panel::Messages));
        QCOMPARE(window.messages()->warning_count(), 1);
        QCOMPARE(window.messages()->error_count(), 1);
    }

    void gdb_waiting_message_names_each_boards_debugger()
    {
        auto settings = fresh_settings();
        MainWindow window(*settings);
        const auto& parts = window.library();
        simnodus::model::Schematic circuit;
        QVERIFY(gdb_waiting_message(circuit, parts, 3333).isEmpty());
        auto add = [&](const char* type, simnodus::model::Point at) {
            simnodus::model::Id id = simnodus::model::no_id;
            simnodus::editing::add_component(circuit, *parts.find(type), at, {}, &id).apply(circuit);
            return QString::fromStdString(circuit.component(id)->designator);
        };
        add("resistor", {0, 0});
        const QString uno = add("arduino_uno", {0, 40});
        QString text = gdb_waiting_message(circuit, parts, 3333);
        QVERIFY2(text.startsWith(QStringLiteral("Waiting for the debugger: connect avr-gdb to localhost:3333.")),
            qPrintable(text));
        QVERIFY(!text.contains(QStringLiteral("arm-none-eabi")) && !text.contains(QStringLiteral("riscv32")));
        const QString pill = add("bluepill", {80, 40});
        const QString c3 = add("esp32c3_devkitm1", {160, 40});
        text = gdb_waiting_message(circuit, parts, 3333);
        QVERIFY2(text.startsWith(QStringLiteral("Waiting for the debuggers: connect avr-gdb to localhost:3333 (%1); "
                                                "STM32CubeIDE or arm-none-eabi-gdb to localhost:3334 (%2); "
                                                "riscv32-esp-elf-gdb to localhost:3335 (%3).")
                                     .arg(uno, pill, c3)),
            qPrintable(text));
    }

    void wifi_dialog_edits_the_projects_network()
    {
        simnodus::model::WifiSettings settings;
        simnodus::ui::WifiDialog dialog(settings);
        auto* ssid = dialog.findChild<QLineEdit*>(QStringLiteral("wifiSsid"));
        auto* passphrase = dialog.findChild<QLineEdit*>(QStringLiteral("wifiPassphrase"));
        auto* gateway = dialog.findChild<QCheckBox*>(QStringLiteral("wifiGateway"));
        auto* forwards = dialog.findChild<QTableWidget*>(QStringLiteral("wifiForwards"));
        auto* buttons = dialog.findChild<QDialogButtonBox*>();
        QVERIFY(ssid && passphrase && gateway && forwards && buttons);
        QCOMPARE(ssid->text(), QStringLiteral("SimNodus"));
        QCOMPARE(passphrase->text(), QStringLiteral("simnodus-wifi"));
        QVERIFY(!gateway->isChecked()); // off by default (DE9c)
        // A WPA2 password that is too short is refused.
        passphrase->setText(QStringLiteral("short"));
        buttons->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        passphrase->setText(QStringLiteral("lab-network"));
        ssid->setText(QStringLiteral("Lab 3"));
        gateway->setChecked(true);
        forwards->insertRow(0);
        forwards->setItem(0, 0, new QTableWidgetItem(QStringLiteral("8080")));
        forwards->setItem(0, 1, new QTableWidgetItem(QStringLiteral("192.168.4.2")));
        forwards->setItem(0, 2, new QTableWidgetItem(QStringLiteral("80")));
        buttons->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        const auto result = dialog.settings();
        QCOMPARE(QString::fromStdString(result.ssid), QStringLiteral("Lab 3"));
        QCOMPARE(QString::fromStdString(result.passphrase), QStringLiteral("lab-network"));
        QVERIFY(result.gateway);
        QCOMPARE(int(result.forwards.size()), 1);
        QCOMPARE(result.forwards.front().host_port, 8080);
        QCOMPARE(QString::fromStdString(result.forwards.front().station), QStringLiteral("192.168.4.2"));
    }

    void shortcut_list_follows_the_menus()
    {
        auto settings = fresh_settings();
        MainWindow window(*settings);
        const QList<ShortcutRow> rows = window.shortcut_rows();
        auto has = [&rows](const QString& keys, const QString& action) {
            return std::any_of(rows.begin(), rows.end(),
                [&](const ShortcutRow& r) { return r.keys == keys && r.action == action; });
        };
        QVERIFY(has(QKeySequence(Qt::Key_F5).toString(QKeySequence::NativeText), QStringLiteral("Run")));
        QVERIFY(has(QKeySequence(Qt::Key_W).toString(QKeySequence::NativeText), QStringLiteral("Wire")));
        QVERIFY(has(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S).toString(QKeySequence::NativeText),
            QStringLiteral("Save As")));
        QVERIFY(has(QKeySequence(Qt::Key_I).toString(QKeySequence::NativeText), QStringLiteral("Current Source")));
        QVERIFY(has(QKeySequence(Qt::SHIFT | Qt::Key_V).toString(QKeySequence::NativeText), QStringLiteral("Voltmeter")));
        QVERIFY(std::any_of(rows.begin(), rows.end(), [](const ShortcutRow& r) { return r.keys == QStringLiteral("Esc"); }));
        QSet<QString> keys; // a key on two actions does nothing in Qt
        for (const ShortcutRow& r : rows) {
            QVERIFY2(!keys.contains(r.keys) || r.keys == QStringLiteral("Esc"), qPrintable(r.keys));
            keys.insert(r.keys);
            QVERIFY2(!r.keys.isEmpty() && !r.action.isEmpty(), qPrintable(r.group));
            QVERIFY(!r.action.contains(QLatin1Char('&')));
        }
    }

    void buzzer_tone_changes_and_stops_without_blocking()
    {
        // SN-228: the tone follows new frequencies and silence at once; closing does not hang.
        QElapsedTimer clock;
        clock.start();
        {
            ToneOutput tone;
            tone.play(523);
            QTest::qWait(120);
            tone.play(1047);
            QTest::qWait(120);
            tone.play(0);
            QTest::qWait(50);
            tone.play(659);
        }
        QVERIFY(clock.elapsed() < 2000);
    }

    void seconds_are_formatted_for_axes()
    {
        QCOMPARE(format_seconds(0.0), QStringLiteral("0"));
        QCOMPARE(format_seconds(2e-3), QStringLiteral("2 ms"));
        QCOMPARE(format_seconds(5e-6), QString::fromUtf8("5 µs"));
        QCOMPARE(format_seconds(1.5), QStringLiteral("1.5 s"));
    }

    void scope_zero_marker_moves_its_channel()
    {
        // SN-189: dragging a channel's 0 V marker sets its offset (fifths of a
        // division); a double-click puts 0 V back at the center.
        ScopeWidget scope;
        scope.resize(600, 400);
        ScopeTrace trace;
        trace.name = QStringLiteral("V(OUT)");
        trace.unit = QStringLiteral("V");
        trace.color = Qt::blue;
        trace.per_division = 1.0;
        ScopeTrace other = trace; // a second channel whose 0 V marker lies on top of the first
        other.name = QStringLiteral("V(IN)");
        other.color = Qt::red;
        scope.set_traces({other, trace});
        scope.set_selected_trace(1); // the selected channel's marker wins where they overlap
        QSignalSpy spy(&scope, &ScopeWidget::offset_changed);
        const QRectF plot = scope.plot_rect();
        const QPointF marker(plot.left() - 3, plot.center().y()); // 0 V at the center
        const double division = plot.height() / ScopeWidget::vertical_divisions;
        auto send = [&scope](QEvent::Type type, QPointF at, Qt::MouseButtons buttons) {
            QMouseEvent event(type, at, scope.mapToGlobal(at), Qt::LeftButton, buttons, Qt::NoModifier);
            QApplication::sendEvent(&scope, &event);
        };
        send(QEvent::MouseButtonPress, marker, Qt::LeftButton);
        send(QEvent::MouseMove, marker + QPointF(0, 2 * division), Qt::LeftButton); // two divisions down
        send(QEvent::MouseButtonRelease, marker + QPointF(0, 2 * division), Qt::NoButton);
        QVERIFY(!spy.isEmpty());
        QCOMPARE(spy.last().at(0).toInt(), 1);
        QCOMPARE(scope.selected_trace(), 1);
        QVERIFY(std::abs(spy.last().at(1).toDouble() - 2.0) < 1e-9); // the center line now shows 2 V
        send(QEvent::MouseButtonDblClick, marker + QPointF(0, 2 * division), Qt::LeftButton);
        QVERIFY(std::abs(spy.last().at(1).toDouble()) < 1e-12);
    }
};

QTEST_MAIN(UiTests)
#include "ui_tests.moc"
