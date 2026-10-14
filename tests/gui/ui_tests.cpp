// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Offscreen GUI smoke tests of the application skeleton (SN-102).
#include "analyzer_window.hpp"
#include "app_settings.hpp"
#include "component_panel.hpp"
#include "main_window.hpp"
#include "messages_panel.hpp"
#include "properties_panel.hpp"
#include "schematic_view.hpp"
#include "scope_widget.hpp"
#include "theme.hpp"

#ifndef Q_MOC_RUN // Qt 6.4's moc cannot parse the C++20 standard headers these pull in
#include "library/component_library.hpp"
#endif

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QSplitter>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

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
        QCOMPARE(all, 20);
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

    void seconds_are_formatted_for_axes()
    {
        QCOMPARE(format_seconds(0.0), QStringLiteral("0"));
        QCOMPARE(format_seconds(2e-3), QStringLiteral("2 ms"));
        QCOMPARE(format_seconds(5e-6), QString::fromUtf8("5 µs"));
        QCOMPARE(format_seconds(1.5), QStringLiteral("1.5 s"));
    }
};

QTEST_MAIN(UiTests)
#include "ui_tests.moc"
