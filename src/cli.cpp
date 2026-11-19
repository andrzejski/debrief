#include "cli.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>

#include "backend.h"
#include "control.h"
#include "services.h"

namespace {

QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err() {
    static QTextStream stream(stderr);
    return stream;
}

const char *usage =
    "Usage: debrief [command]\n"
    "\n"
    "Records your voice while you debug, transcribes it, and takes a screenshot\n"
    "whenever you say \"screenshot\". Each session is a folder under\n"
    "~/Documents/Debrief, named for when it started and what it was about, with\n"
    "transcript.md, session.json, and the screenshots; the recording sits beside\n"
    "it as a .wav of the same name.\n"
    "\n"
    "Commands:\n"
    "  open                 Show the window (the default)\n"
    "  start                Start a session, launching Debrief if needed\n"
    "  stop                 Stop the session and finish its transcript\n"
    "  toggle               Start or stop\n"
    "  shot                 Take a screenshot in the running session now\n"
    "  status [--json]      Whether a session is recording\n"
    "  prompt               Print the prompt that asks Claude for the report\n"
    "  bar install|remove   Add or remove the Omarchy bar widget\n"
    "  skill install        Install the agent skill in ~/.claude/skills/debrief\n"
    "\n"
    "Commands that talk to a session take --json, exit 0 or 1, and report errors on stderr.\n";

int report(const QJsonObject &status, bool json) {
    if (json) {
        out() << QJsonDocument(status).toJson(QJsonDocument::Compact) << "\n";
    } else if (!status.value("ok").toBool(true)) {
        // Shown below.
    } else if (!status.value("running").toBool()) {
        out() << "Debrief isn't running.\n";
    } else {
        const QString state = status.value("state").toString();
        out() << (state == "recording" ? "Recording" : state == "finishing" ? "Finishing the transcript" : "Idle");
        if (!status.value("folder").toString().isEmpty())
            out() << ": " << status.value("folder").toString() << " (" << status.value("screenshots").toInt()
                  << " screenshots)";
        out() << "\n";
    }
    if (!status.value("ok").toBool(true)) {
        err() << status.value("error").toString() << "\n";
        return 1;
    }
    return 0;
}

int drive(const QString &command, bool json) {
    if (auto reply = Control::send(command))
        return report(*reply, json);

    if (command == "status")
        return report({{"ok", true}, {"running", false}, {"state", "idle"}}, json);
    if (command != "start" && command != "toggle") {
        return report({{"ok", false}, {"running", false}, {"state", "idle"},
                       {"error", "Debrief isn't running, so there is no session to " + command + "."}},
                      json);
    }

    // No app yet: launch one that starts recording without opening a window.
    if (!startDetachedQuietly(QCoreApplication::applicationFilePath(), {"open", "--start", "--hidden"}))
        return report({{"ok", false}, {"error", "Couldn't launch Debrief."}}, json);
    QDeadlineTimer deadline(5000);
    while (!deadline.hasExpired()) {
        QThread::msleep(100);
        if (auto reply = Control::send("status"); reply && reply->value("state").toString() != "idle")
            return report(*reply, json);
    }
    return report({{"ok", false}, {"error", "Debrief didn't start recording."}}, json);
}

int runQuiet(const QString &program, const QStringList &arguments) {
    QProcess process;
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(program, arguments);
    if (!process.waitForFinished(30000))
        return -1;
    return process.exitCode();
}

int bar(const QString &action) {
#ifdef Q_OS_WIN
    Q_UNUSED(action);
    err() << "The bar widget is for Omarchy. On Windows, Debrief sits in the notification area instead.\n";
    return 1;
#endif
    const QString target = QDir::homePath() + "/.config/omarchy/plugins/debrief-bar";
    if (action == "remove") {
        runQuiet("omarchy", {"plugin", "disable", "debrief.bar"});
        QFile::remove(target);
        out() << "Removed the Debrief bar widget.\n";
        return 0;
    }
    if (action != "install") {
        err() << "Usage: debrief bar install|remove\n";
        return 1;
    }
    const QString source = Cli::pluginSource(QCoreApplication::applicationDirPath());
    if (source.isEmpty()) {
        err() << "Couldn't find the bar widget that comes with Debrief.\n";
        return 1;
    }
    QDir().mkpath(QFileInfo(target).absolutePath());
    // A link, so an update to Debrief updates the widget too.
    QFileInfo existing(target);
    if (existing.isSymLink() || existing.isFile())
        QFile::remove(target);
    else if (existing.isDir()) {
        err() << target << " already exists and isn't Debrief's link; remove it first.\n";
        return 1;
    }
    if (!QFile::link(source, target)) {
        err() << "Couldn't link " << source << " to " << target << ".\n";
        return 1;
    }
    if (QStandardPaths::findExecutable("omarchy").isEmpty()) {
        out() << "Linked the widget to " << target << "; Omarchy isn't installed, so it isn't placed in a bar.\n";
        return 0;
    }
    runQuiet("omarchy-shell", {"shell", "rescanPlugins"});
    // The link's target may have changed under the shell, which only notices
    // saves inside its own config folder.
    runQuiet("omarchy-shell", {"shell", "reloadPlugins"});
    if (runQuiet("omarchy", {"plugin", "enable", "debrief.bar", "--section", "right"}) != 0) {
        err() << "Linked the widget, but Omarchy couldn't add it to the bar.\n";
        return 1;
    }
    out() << "Added the Debrief widget to the right of the bar. Click it to start or stop a session.\n";
    return 0;
}

int skill() {
    const QString folder = Backend::defaultSkillFolder();
    if (!Backend::installSkill(folder)) {
        err() << "Couldn't install the skill in " << folder << ".\n";
        return 1;
    }
    out() << "Installed the Debrief skill in " << folder << ".\n";
    return 0;
}

} // namespace

QString Cli::pluginSource(const QString &applicationDir) {
    QStringList candidates{
        applicationDir + "/../share/debrief/omarchy-plugin/debrief-bar",
        // A run from build/ in the source tree.
        applicationDir + "/../share/omarchy-plugin/debrief-bar",
    };
    const QString located =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "debrief/omarchy-plugin/debrief-bar",
                               QStandardPaths::LocateDirectory);
    if (!located.isEmpty())
        candidates.prepend(located);
    for (const QString &candidate : candidates)
        if (QFileInfo::exists(candidate + "/manifest.json"))
            return QFileInfo(candidate).canonicalFilePath();
    return {};
}

bool Cli::wantsWindow(const QStringList &arguments) {
    return arguments.size() < 2 || arguments.at(1) == "open" || arguments.at(1).startsWith("--start")
        || arguments.at(1) == "--hidden";
}

int Cli::run(const QStringList &arguments) {
    const QString command = arguments.value(1);
    const bool json = arguments.contains("--json");
    if (command == "--help" || command == "-h" || command == "help") {
        out() << usage;
        return 0;
    }
    if (command == "--version") {
        out() << "debrief " << QCoreApplication::applicationVersion() << "\n";
        return 0;
    }
    if (QStringList{"start", "stop", "toggle", "shot", "status"}.contains(command))
        return drive(command, json);
    if (command == "prompt") {
        out() << Backend::reportPrompt() << "\n";
        return 0;
    }
    if (command == "bar")
        return bar(arguments.value(2));
    if (command == "skill" && arguments.value(2) == "install")
        return skill();
    err() << "Unknown command: " << command << "\n\n" << usage;
    return 1;
}
