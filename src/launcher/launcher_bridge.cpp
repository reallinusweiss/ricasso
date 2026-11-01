// ============================================================================
// launcher_bridge.cpp — Bridge connecting Qt6 launcher to main.cpp
// ============================================================================

#include "platform/launcher_ui.h"
#include "launcher/qt_launcher_window.h"

#include <QApplication>
#include <memory>

LaunchConfig show_launcher(BinarySelector& selector) {
    int argc = 1;
    static char arg0[] = "swordfare";
    static char* argv[] = { arg0, nullptr };

    QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    std::unique_ptr<QApplication> owned_app;
    if (!app) {
        owned_app = std::make_unique<QApplication>(argc, argv);
        app = owned_app.get();
    }

    swordfare::launcher::QtLauncherWindow window(selector);
    window.show();

    app->exec();

    LaunchConfig cfg = window.get_launch_config();
    cfg.should_launch = window.should_launch();
    return cfg;
}
