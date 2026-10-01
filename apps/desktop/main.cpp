// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "document_window.hpp"
#include <QApplication>
#include <QTimer>
#include <cstdio>

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    QApplication::setStyle("Fusion");
    DocumentWindow window;
    window.show();
    const auto arguments = application.arguments();
    if(arguments.size() == 7 && arguments[1] == "--preview-acceptance-root" && arguments[3] == "--resource-root" && arguments[5] == "--report") {
        QTimer::singleShot(200, &window, [&] { window.runPreviewAcceptance(arguments[2], arguments[4], arguments[6]); });
    } else if(arguments.size() == 5 && arguments[3] == "--report" &&
        (arguments[1] == "--acceptance-root" || arguments[1] == "--instance-acceptance-root" || arguments[1] == "--history-acceptance-root" || arguments[1] == "--resistance-acceptance-root" || arguments[1] == "--capacitance-acceptance-root" || arguments[1] == "--inspection-acceptance-root")) {
        QTimer::singleShot(200, &window, [&] {
            if(arguments[1] == "--inspection-acceptance-root") window.runInspectionAcceptance(arguments[2], arguments[4]);
            else if(arguments[1] == "--capacitance-acceptance-root") window.runCapacitanceAcceptance(arguments[2], arguments[4]);
            else if(arguments[1] == "--resistance-acceptance-root") window.runResistanceAcceptance(arguments[2], arguments[4]);
            else if(arguments[1] == "--history-acceptance-root") window.runHistoryAcceptance(arguments[2], arguments[4]);
            else if(arguments[1] == "--instance-acceptance-root") window.runInstanceAcceptance(arguments[2], arguments[4]);
            else window.runAcceptance(arguments[2], arguments[4]);
        });
    } else if(arguments.size() != 1) {
        std::fprintf(stderr, "Usage: simnodus_document_editor [--acceptance-root|--instance-acceptance-root|--history-acceptance-root|--resistance-acceptance-root|--capacitance-acceptance-root|--inspection-acceptance-root ROOT --report NEW_REPORT] or --preview-acceptance-root ROOT --resource-root ART_ROOT --report NEW_REPORT\n");
        return 2;
    }
    return application.exec();
}
