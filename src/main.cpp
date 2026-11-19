// debrief — records your voice while you debug and takes a screenshot whenever
// you say "screenshot", tying each one to the words around it.

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <memory>

#include "backend.h"
#include "cli.h"
#include "control.h"
#include "theme.h"
#include "translator.h"

#ifdef DEBRIEF_NATIVE
#include <QApplication>
#include "native/tray.h"
#endif
#ifdef Q_OS_WIN
#include <cstdio>
#include <windows.h>

// A windowed program has no console of its own. Run from one, a command
// writes to it; redirected or piped output is left as it was given.
static void useParentConsole() {
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output && output != INVALID_HANDLE_VALUE && GetFileType(output) != FILE_TYPE_UNKNOWN)
        return;
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
}

// A windowed program's warnings go nowhere, so they are kept in a file that
// can be read, or sent along, when something doesn't work:
// %LOCALAPPDATA%\debrief\debrief.log, started over once it is large.
static QString logPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/debrief.log";
}

static void logToFile(QtMsgType type, const QMessageLogContext &, const QString &message) {
    static QMutex mutex;
    static QFile file;
    const QMutexLocker locker(&mutex);
    if (!file.isOpen()) {
        const QString path = logPath();
        QDir().mkpath(QFileInfo(path).absolutePath());
        file.setFileName(path);
        const bool large = QFileInfo(path).size() > 1024 * 1024;
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | (large ? QIODevice::Truncate : QIODevice::Append)))
            return;
    }
    const char *kind = type == QtDebugMsg ? "debug" : type == QtInfoMsg ? "info" : type == QtWarningMsg ? "warning"
                                                                                                         : "error";
    file.write(QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8() + " " + kind + ": " + message.toUtf8()
               + "\n");
    file.flush();
}
#endif

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);

    // Commands need no display, so they never touch QGuiApplication, which
    // aborts without one.
    if (!Cli::wantsWindow(arguments)) {
#ifdef Q_OS_WIN
        useParentConsole();
#endif
        QCoreApplication app(argc, argv);
        app.setApplicationName("debrief");
        app.setApplicationVersion("1.0.0");
        return Cli::run(app.arguments());
    }

    const bool startNow = arguments.contains("--start");
    const bool hidden = arguments.contains("--hidden");

    // One Debrief at a time: a second launch hands over to the first.
    auto handedOver = [&] {
        if (!Control::send("status"))
            return false;
        if (startNow)
            Control::send("start");
        if (!hidden)
            Control::send("show");
        return true;
    };
    // Asked before there is an application where that spares connecting to
    // the display; Windows pipes want the application first.
#ifndef Q_OS_WIN
    if (handedOver())
        return 0;
#endif

#ifdef DEBRIEF_NATIVE
    // The notification area icon and its menu are widgets.
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif
    app.setApplicationName("debrief");
    app.setApplicationVersion("1.0.0");
#ifdef Q_OS_WIN
    if (handedOver())
        return 0;
    qInstallMessageHandler(logToFile);
    qInfo("Debrief %s starting, Qt %s", qPrintable(app.applicationVersion()), qVersion());
#endif

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
#ifdef Q_OS_WIN
    // Qt's QML modules sit in qml\ beside the program.
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml");
#endif
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty()) {
#ifdef Q_OS_WIN
        // Rather than nothing at all: say so, and where the reason is.
        const QString text = "Debrief couldn't open its window. The reason is in\n"
                             + QDir::toNativeSeparators(logPath());
        MessageBoxW(nullptr, reinterpret_cast<const wchar_t *>(text.utf16()), L"Debrief", MB_OK | MB_ICONERROR);
#endif
        return 1;
    }
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

    // Where there is a notification area (Windows), Debrief stays in it with
    // its window closed and no session, until it is told to quit.
    bool staying = false;
#ifdef DEBRIEF_NATIVE
    std::unique_ptr<Tray> tray;
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    // Always on Windows, even started at sign-in before the taskbar is up:
    // the icon appears once there is somewhere to put it. On macOS it is the
    // menu bar.
    const bool wantsTray = true;
#else
    const bool wantsTray = qEnvironmentVariableIsSet("DEBRIEF_TRAY") && Tray::available();
#endif
    if (wantsTray) {
        staying = true;
        tray = std::make_unique<Tray>(&backend);
        QObject::connect(tray.get(), &Tray::showRequested, &control, &Control::showRequested);
        QObject::connect(tray.get(), &Tray::quitRequested, window, [&, window] {
            // What has been recorded is saved first.
            window->setProperty("quitting", true);
            window->hide();
            if (backend.recording())
                backend.stop();
            else if (backend.idle())
                app.quit();
        });
        QObject::connect(window, &QWindow::visibleChanged, tray.get(), [&, window] {
            if (!window->isVisible() && !window->property("quitting").toBool())
                tray->sayStillRunning();
        });
    }
#endif

#ifdef Q_OS_MACOS
    // Clicking the Dock icon with the window closed brings the window back.
    // Not in the first moments, so a hidden start stays hidden.
    QElapsedTimer sinceStart;
    sinceStart.start();
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, window, [&, window](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive && sinceStart.elapsed() > 3000 && !window->isVisible()
            && !window->property("quitting").toBool())
            emit control.showRequested();
    });
#endif

    auto quitIfDone = [&] {
        if (staying && !window->property("quitting").toBool())
            return;
        if (!window->isVisible() && backend.idle())
            app.quit();
    };
    QObject::connect(window, &QWindow::visibleChanged, &app, [&] { QTimer::singleShot(0, &app, quitIfDone); });
    QObject::connect(&backend, &Backend::stateChanged, &app, [&] { QTimer::singleShot(0, &app, quitIfDone); });

    return app.exec();
}
