// debrief — records your voice while you debug and takes a screenshot whenever
// you say "screenshot this", tying each one to the words around it.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "control.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);

    // Commands need no display, so they never touch QGuiApplication, which
    // aborts without one.
    if (!Cli::wantsWindow(arguments)) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("debrief");
        app.setApplicationVersion("0.1.0");
        return Cli::run(app.arguments());
    }

    const bool startNow = arguments.contains("--start");
    const bool hidden = arguments.contains("--hidden");

    // One Debrief at a time: a second launch hands over to the first.
    if (Control::send("status")) {
        if (startNow)
            Control::send("start");
        if (!hidden)
            Control::send("show");
        return 0;
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("debrief");
    app.setApplicationVersion("0.1.0");

    // Matches the window to debrief.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon. The compiled-in copy covers
    // a run from build/, and needs qt6-svg's icon engine.
    app.setDesktopFileName("debrief");
    app.setWindowIcon(QIcon::fromTheme("debrief", QIcon(":/debrief.svg")));
    // A session keeps recording with its window closed; the app goes once
    // there is neither a window nor a session.
    app.setQuitOnLastWindowClosed(false);

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;
    Control control(&backend);
    control.listen();

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("startHidden", hidden);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());

    QObject::connect(&control, &Control::showRequested, window, [window] {
        window->show();
        window->raise();
        window->requestActivate();
    });

    if (startNow) {
        backend.start();
        // A session that couldn't start says why in the window.
        if (backend.idle())
            window->show();
    }

    auto quitIfDone = [&] {
        if (!window->isVisible() && backend.idle())
            app.quit();
    };
    QObject::connect(window, &QWindow::visibleChanged, &app, [&] { QTimer::singleShot(0, &app, quitIfDone); });
    QObject::connect(&backend, &Backend::stateChanged, &app, [&] { QTimer::singleShot(0, &app, quitIfDone); });

    return app.exec();
}
