#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

#include <QImage>
#include <QJsonArray>
#include <QSignalSpy>
#include <QtEndian>
#include <cmath>

#include "backend.h"
#include "cli.h"
#include "control.h"
#include "segmenter.h"
#include "session.h"
#include "theme.h"
#include "translator.h"

namespace {

bool write(const QString &path, const QByteArray &contents) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

int lines(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll().count('\n') : 0;
}

// A hyprctl that answers with response.json beside it and counts its calls.
QString fakeHyprctl(const QString &directory) {
    const QString command = directory + "/hyprctl";
    write(command, "#!/bin/bash\necho >> '" + directory.toUtf8() + "/calls'\ncat '" + directory.toUtf8()
                       + "/response.json'\n");
    QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return command;
}

QJsonObject readCatalog(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

// The placeholders in a text, sorted: %1 to %99, and the count (%n, %Ln).
QStringList placeholders(const QString &text) {
    static const QRegularExpression placeholder("%L?(?:[1-9][0-9]?|n)");
    QStringList found;
    for (auto it = placeholder.globalMatch(text); it.hasNext();)
        found << it.next().captured();
    found.sort();
    return found;
}

// Every text the sources hand to qsTr() or tr(), and whether a call passes it
// a count. It walks the code a token at a time, so a string never hides a
// comment or the other way round, C++'s adjacent literals join, and QML's
// single quotes count. A text that isn't a literal can't be in a catalog.
QMap<QString, bool> translatables(const QString &folder, QStringList *problems) {
    QMap<QString, bool> found;
    for (const QFileInfo &info : QDir(folder).entryInfoList({"*.qml", "*.js", "*.cpp", "*.h"}, QDir::Files)) {
        QFile file(info.filePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QString code = QString::fromUtf8(file.readAll());
        qsizetype i = 0;
        auto literal = [&](QString *text) {
            const QChar quote = code[i++];
            for (; i < code.size() && code[i] != quote; ++i) {
                if (code[i] == '\\' && i + 1 < code.size()) {
                    const QChar escaped = code[++i];
                    const int digits = escaped == 'u' ? 4 : escaped == 'x' ? 2 : 0;
                    bool hex = false;
                    const ushort unit = code.mid(i + 1, digits).toUShort(&hex, 16);
                    if (digits && hex) {
                        *text += QChar(unit);
                        i += digits;
                    } else {
                        *text += escaped == 'n' ? QChar('\n') : escaped == 't' ? QChar('\t') : escaped;
                    }
                } else {
                    *text += code[i];
                }
            }
            ++i;
        };
        auto skipSpace = [&] {
            while (i < code.size() && code[i].isSpace())
                ++i;
        };
        while (i < code.size()) {
            const QChar c = code[i];
            if (code.mid(i, 2) == "//") {
                i = code.indexOf('\n', i);
                if (i < 0)
                    break;
            } else if (code.mid(i, 2) == "/*") {
                i = code.indexOf("*/", i + 2);
                if (i < 0)
                    break;
                i += 2;
            } else if (c == '"' || c == '\'') {
                QString skipped;
                literal(&skipped);
            } else if (c.isLetter() || c == '_') {
                const qsizetype start = i;
                while (i < code.size() && (code[i].isLetterOrNumber() || code[i] == '_'))
                    ++i;
                const QString word = code.mid(start, i - start);
                skipSpace();
                if ((word != "qsTr" && word != "tr") || i >= code.size() || code[i] != '(')
                    continue;
                ++i;
                skipSpace();
                if (i >= code.size() || (code[i] != '"' && code[i] != '\'')) {
                    *problems << info.fileName() + ": not a literal: " + code.mid(start, 40);
                    continue;
                }
                QString text;
                do {
                    literal(&text);
                    skipSpace();
                } while (i < code.size() && code[i] == '"');
                // The arguments after the text, to the call's closing parenthesis.
                int depth = 0, commas = 0;
                while (i < code.size()) {
                    const QChar d = code[i];
                    if (d == '"' || d == '\'') {
                        QString skipped;
                        literal(&skipped);
                        continue;
                    }
                    if (d == '(' || d == '[' || d == '{')
                        ++depth;
                    else if ((d == ')' || d == ']' || d == '}') && depth-- == 0)
                        break;
                    else if (d == ',' && depth == 0)
                        ++commas;
                    ++i;
                }
                found[text] = found.value(text) || commas >= 2;
            } else {
                ++i;
            }
        }
    }
    return found;
}


// ---- Debrief's fakes ---------------------------------------------------------

constexpr int kRate = 16000;

// Speech-like sound: a loud tone. Silence: a quiet hiss.
QByteArray tone(double seconds, int amplitude = 8000) {
    QByteArray pcm(qsizetype(seconds * kRate) * 2, '\0');
    auto *s = reinterpret_cast<qint16 *>(pcm.data());
    for (qsizetype i = 0; i < pcm.size() / 2; ++i)
        s[i] = qint16(amplitude * std::sin(2 * M_PI * 220 * double(i) / kRate));
    return pcm;
}
QByteArray quiet(double seconds) { return tone(seconds, 40); }

QList<Segmenter::Utterance> segment(Segmenter &segmenter, const QByteArray &pcm) {
    QList<Segmenter::Utterance> found;
    // In uneven pieces, as a pipe delivers them.
    for (qsizetype at = 0; at < pcm.size();) {
        const qsizetype n = std::min<qsizetype>(pcm.size() - at, 2 * 1237);
        found += segmenter.feed(reinterpret_cast<const qint16 *>(pcm.constData() + at), n / 2);
        at += n;
    }
    return found;
}

class FakeAudio : public AudioSource {
public:
    bool running = false;
    void start() override { running = true; }
    void stop() override {
        running = false;
        QTimer::singleShot(0, this, [this] { emit stopped(); });
    }
    void push(const QByteArray &pcm) {
        for (qsizetype at = 0; at < pcm.size(); at += 3200)
            emit audio(pcm.mid(at, 3200));
    }
};

// Writes a tiny PNG for every capture.
class FakeScreens : public Screenshotter {
public:
    int captures = 0;
    void capture(int id, const QString &path) override {
        ++captures;
        QTimer::singleShot(0, this, [this, id, path] {
            QImage image(16, 9, QImage::Format_RGB32);
            image.fill(Qt::red);
            emit captured(id, image.save(path), {});
        });
    }
};

// Answers each file with the next of its words, when asked to.
class FakeTranscriber : public Transcriber {
public:
    QStringList words;
    QList<int> waiting;
    bool automatic = true;
    QString unavailableReason() const override { return {}; }
    void transcribe(int id, const QString &path) override {
        if (!QFile::exists(path))
            qWarning() << "no such chunk" << path;
        waiting << id;
        if (automatic)
            QTimer::singleShot(0, this, [this] { answer(); });
    }
    void answer() {
        if (waiting.isEmpty())
            return;
        const int id = waiting.takeFirst();
        emit transcribed(id, true, words.isEmpty() ? QString() : words.takeFirst(), {});
    }
};

// Answers with its words, as Claude would a moment later.
class FakeNamer : public Namer {
public:
    QString words = "Save button grey.";
    QStringList asked;
    void name(const QString &transcript) override {
        asked << transcript;
        QTimer::singleShot(0, this, [this] { emit named(words); });
    }
};

struct Rig {
    QTemporaryDir dir;
    FakeAudio *audio = new FakeAudio;
    FakeScreens *screens = new FakeScreens;
    FakeTranscriber *transcriber = new FakeTranscriber;
    FakeNamer *namer = new FakeNamer;
    Backend backend{audio, screens, transcriber, namer, dir.filePath("sessions"), dir.filePath("run/state.json")};
    Rig() { backend.setNotify(false); }
};

QJsonObject readJson(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

} // namespace

class DebriefTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QQuickStyle::setStyle("Material"); }

    void segmenterFindsUtterancesBetweenPauses() {
        Segmenter segmenter(kRate);
        const auto found = segment(segmenter, quiet(1) + tone(1.5) + quiet(1.5) + tone(1) + quiet(1.5));
        QCOMPARE(found.size(), 2);
        // Each starts a little before its sound and ends a little after it.
        QVERIFY(found[0].start >= kRate * 0.6 && found[0].start <= kRate * 1.0);
        QVERIFY(found[0].end >= kRate * 2.5 && found[0].end <= kRate * 2.9);
        QVERIFY(found[1].start >= found[0].end);
        QVERIFY(found[1].start >= kRate * 3.6 && found[1].start <= kRate * 4.0);
        QVERIFY(!segmenter.flush());
    }

    void segmenterIgnoresAClick() {
        Segmenter segmenter(kRate);
        QVERIFY(segment(segmenter, quiet(1) + tone(0.03) + quiet(2)).isEmpty());
        QVERIFY(!segmenter.inSpeech());
    }

    void segmenterCutsALongMonologue() {
        Segmenter segmenter(kRate);
        const auto found = segment(segmenter, quiet(0.5) + tone(40));
        QCOMPARE(found.size(), 2);
        QCOMPARE(found[0].end - found[0].start, segmenter.maxLength());
        QCOMPARE(found[1].start, found[0].end);
        const auto rest = segmenter.flush();
        QVERIFY(rest);
        QCOMPARE(rest->start, found[1].end);
        QCOMPARE(rest->end, segmenter.position());
    }

    void recognizesAskingForAScreenshot() {
        // The word alone is enough, however it is said or written down.
        for (const char *yes : {"Screenshot.", "OK, screen shot that!", "can you take a screenshot",
                                "SCREENSHOT THIS", "the button is grey, screenshot, and then", "Screen-shot",
                                "two screenshots of this", "the screenshot feature is broken"})
            QVERIFY2(Debrief::asksForScreenshot(yes), yes);
        for (const char *no : {"this screen", "a shot of the screen", "the screen shows nothing", ""})
            QVERIFY2(!Debrief::asksForScreenshot(no), no);
    }

    void namesAreTwoOrThreeWords() {
        QCOMPARE(Debrief::slug("Approve flow stalls."), QString("approve-flow-stalls"));
        QCOMPARE(Debrief::slug("  Login / Timeout\n"), QString("login-timeout"));
        QCOMPARE(Debrief::slug("one two three four"), QString("one-two-three"));
        // A sentence is not a name, and neither is nothing.
        QVERIFY(Debrief::slug("I'm sorry, I can't name this session without a transcript").isEmpty());
        QVERIFY(Debrief::slug("...").isEmpty());

        // Without a name given, the words said most, in the order they came.
        QCOMPARE(Debrief::describe({"the save button stays grey", "I click the save button", "okay screenshot this",
                                    "the button is still grey"}),
                 QString("save-button-grey"));
        QCOMPARE(Debrief::describe({}), QString("nothing-said"));
        QCOMPARE(Debrief::describe({"Um.", "okay"}), QString("nothing-said"));
    }

    void timecodes() {
        QCOMPARE(Debrief::timecode(0), QString("00:00:00"));
        QCOMPARE(Debrief::timecode(61999), QString("00:01:01"));
        QCOMPARE(Debrief::timecode(3723000), QString("01:02:03"));
    }

    void wavHeaderDescribesTheAudio() {
        const QByteArray header = Debrief::WavWriter::header(32000, kRate);
        QCOMPARE(header.size(), 44);
        QCOMPARE(header.left(4), QByteArray("RIFF"));
        QCOMPARE(qFromLittleEndian<quint32>(header.constData() + 40), 32000u);
        QCOMPARE(qFromLittleEndian<quint32>(header.constData() + 24), quint32(kRate));
    }

    void sessionLinksScreenshotsToTheirWords() {
        SessionLog log;
        log.started = QDateTime(QDate(2026, 11, 13), QTime(10, 55));
        log.durationMs = 30000;
        log.finished = true;
        log.speech = {{1000, 3000, "I open the settings page", Speech::Done, {}},
                      {4000, 6000, "the save button is grey, screenshot this", Speech::Done, {}},
                      {8000, 9000, "", Speech::Failed, "timeout"}};
        log.shots = {{1, 6000, "shot-01-00-00-06.png", "voice", 1},
                     {2, 20000, "shot-02-00-00-20.png", "manual", -1}};
        log.title = "save button grey";
        log.audio = "../2026-11-13_10-55-00_save-button-grey.wav";

        const QString md = log.markdown();
        const auto said = md.indexOf("the save button is grey");
        const auto image = md.indexOf("![Screenshot 1 at 00:00:06](shot-01-00-00-06.png)");
        QVERIFY(said > 0 && image > said);
        QVERIFY(md.indexOf("(not transcribed: timeout)") > image);
        // A screenshot with no words of its own comes where its time puts it.
        QVERIFY(md.indexOf("Screenshot 2 at 00:00:20") > md.indexOf("(not transcribed"));
        QVERIFY(md.startsWith("# Debrief session: save button grey, 2026-11-13 10:55\n"));
        QVERIFY(md.contains("(../2026-11-13_10-55-00_save-button-grey.wav)"));

        const QJsonObject json = log.json();
        QCOMPARE(json.value("title").toString(), QString("save button grey"));
        QCOMPARE(json.value("audio").toString(), QString("../2026-11-13_10-55-00_save-button-grey.wav"));
        const QJsonObject shot = json.value("screenshots").toArray().at(0).toObject();
        QCOMPARE(shot.value("transcript_index").toInt(), 1);
        QCOMPARE(shot.value("said").toString(), QString("the save button is grey, screenshot this"));
        QVERIFY(shot.value("context").toString().startsWith("I open the settings page"));
        QCOMPARE(json.value("transcript").toArray().at(1).toObject().value("screenshots").toArray(),
                 QJsonArray({1}));
        QCOMPARE(json.value("transcript").toArray().at(2).toObject().value("status").toString(), QString("failed"));

        QTemporaryDir dir;
        QString error;
        QVERIFY(log.save(dir.path(), &error));
        QCOMPARE(readJson(dir.filePath("session.json")).value("duration_ms").toInt(), 30000);
    }

    void sessionKeepsAScreenshotOnlyWhenAskedFor() {
        Rig rig;
        rig.transcriber->words = {"the save button stays grey", "okay there, screenshot"};
        rig.backend.start();
        QVERIFY(rig.backend.recording());
        QVERIFY(rig.audio->running);
        const QString began = rig.backend.folder();
        QVERIFY(began.startsWith(rig.dir.filePath("sessions")));
        // The recording is beside the folder from the start, not in it.
        QVERIFY(QFile::exists(began + ".wav"));
        QCOMPARE(readJson(rig.dir.filePath("run/state.json")).value("state").toString(), QString("recording"));

        rig.audio->push(quiet(1) + tone(1.5) + quiet(1.5) + tone(1) + quiet(1.5));
        QCOMPARE(rig.backend.elapsed(), 6);
        // A candidate screenshot for each utterance, the moment its words end.
        QCOMPARE(rig.screens->captures, 2);
        QTRY_COMPARE(rig.backend.shotCount(), 1);

        rig.backend.stop();
        QCOMPARE(rig.backend.state(), QString("finishing"));
        QSignalSpy moved(&rig.backend, &Backend::folderChanged);
        QTRY_VERIFY(rig.backend.idle());

        // The folder ends with what the session was about, and so does the
        // recording beside it.
        const QString folder = rig.backend.folder();
        QCOMPARE(folder, began + "_save-button-grey");
        QCOMPARE(moved.size(), 1);
        QCOMPARE(rig.namer->asked, QStringList{"the save button stays grey\nokay there, screenshot"});
        QVERIFY(!QFileInfo::exists(began) && !QFileInfo::exists(began + ".wav"));
        QCOMPARE(QDir(rig.dir.filePath("sessions")).entryList(QDir::AllEntries | QDir::NoDotAndDotDot),
                 QStringList({QFileInfo(folder).fileName(), QFileInfo(folder).fileName() + ".wav"}));
        QCOMPARE(readJson(rig.dir.filePath("run/state.json")).value("folder").toString(), folder);

        const SessionLog &log = rig.backend.log();
        QCOMPARE(log.title, QString("save button grey"));
        QCOMPARE(log.speech.size(), 2);
        QCOMPARE(log.shots.size(), 1);
        QCOMPARE(log.shots[0].speech, 1);
        QCOMPARE(log.shots[0].source, QString("voice"));
        QVERIFY(QFile::exists(folder + "/" + log.shots[0].file));
        // One folder to hand over: the transcript, its data, and the screenshot.
        // The unwanted candidate and the cut audio are gone with the work folder.
        QCOMPARE(QDir(folder).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot),
                 QStringList({"session.json", log.shots[0].file, "transcript.md"}));

        QVERIFY(log.finished);
        QCOMPARE(readJson(folder + "/session.json").value("screenshots").toArray().size(), 1);
        QVERIFY(QFile::exists(folder + "/transcript.md"));
        QCOMPARE(readJson(folder + "/session.json").value("audio").toString(),
                 "../" + QFileInfo(folder).fileName() + ".wav");
        QFile wav(folder + ".wav");
        QVERIFY(wav.open(QIODevice::ReadOnly));
        QCOMPARE(wav.size(), 44 + qint64(6.5 * kRate) * 2);
        QCOMPARE(qFromLittleEndian<quint32>(wav.read(44).constData() + 40), quint32(6.5 * kRate) * 2);
        QCOMPARE(readJson(rig.dir.filePath("run/state.json")).value("state").toString(), QString("idle"));
    }

    void stoppingWaitsForTheLastWords() {
        Rig rig;
        rig.transcriber->automatic = false;
        rig.transcriber->words = {"screenshot this"};
        QSignalSpy finished(&rig.backend, &Backend::finished);
        rig.backend.start();
        // Still talking when the session stops: the last words are cut there.
        rig.audio->push(quiet(1) + tone(2));
        rig.backend.stop();
        QTRY_COMPARE(rig.transcriber->waiting.size(), 1);
        QTest::qWait(50);
        QCOMPARE(rig.backend.state(), QString("finishing"));
        QCOMPARE(rig.backend.pendingCount(), 1);
        rig.transcriber->answer();
        QTRY_VERIFY(rig.backend.idle());
        QCOMPARE(finished.size(), 1);
        QCOMPARE(rig.backend.shotCount(), 1);
    }

    void sessionNamesItselfWhenNobodyElseDoes() {
        Rig rig;
        // No answer, as when Claude isn't installed or takes too long.
        rig.namer->words.clear();
        rig.transcriber->words = {"the invoice total is wrong", "the total ignores the invoice discount"};
        rig.backend.start();
        const QString began = rig.backend.folder();
        rig.audio->push(quiet(1) + tone(1.5) + quiet(1.5) + tone(1) + quiet(1.5));
        rig.backend.stop();
        QTRY_VERIFY(rig.backend.idle());
        QCOMPARE(rig.backend.folder(), began + "_invoice-total-wrong");
        QVERIFY(QFile::exists(began + "_invoice-total-wrong.wav"));

        // Nothing said: nothing to ask about.
        Rig silent;
        silent.backend.start();
        silent.audio->push(quiet(2));
        silent.backend.stop();
        QTRY_VERIFY(silent.backend.idle());
        QVERIFY(silent.backend.folder().endsWith("_nothing-said"));
        QVERIFY(silent.namer->asked.isEmpty());
    }

    // Asks the real Claude, so it only runs when told to: DEBRIEF_LIVE=1 bin/test
    void claudeNamesASession() {
        if (!qEnvironmentVariableIsSet("DEBRIEF_LIVE"))
            QSKIP("Set DEBRIEF_LIVE=1 to ask Claude for a name.");
        ClaudeNamer namer;
        QSignalSpy named(&namer, &Namer::named);
        namer.name("the save button stays grey\nI click save and nothing happens\nscreenshot");
        QVERIFY(named.wait(40000));
        const QString about = Debrief::slug(named.first().first().toString());
        qInfo() << "Claude named it" << about;
        QVERIFY(about.count('-') >= 1 && about.count('-') <= 2);
    }

    void screenshotByHandBelongsToTheWordsBeingSaid() {
        Rig rig;
        rig.transcriber->words = {"look at this dialog"};
        rig.backend.start();
        rig.audio->push(quiet(1) + tone(0.5));
        QVERIFY(rig.backend.speaking());
        rig.backend.screenshotNow();
        rig.audio->push(tone(0.5) + quiet(1.5));
        QTRY_COMPARE(rig.backend.shotCount(), 1);
        QTRY_COMPARE(rig.backend.pendingCount(), 0);
        const Shot shot = rig.backend.log().shots.first();
        QCOMPARE(shot.source, QString("manual"));
        QCOMPARE(shot.speech, 0);
        QCOMPARE(rig.backend.log().speech.size(), 1);
        rig.backend.stop();
        QTRY_VERIFY(rig.backend.idle());
    }

    void controlAnswersOnItsSocket() {
        Rig rig;
        Control control(&rig.backend, rig.dir.filePath("run/control.sock"));
        QVERIFY(control.listen());
        auto ask = [&](const QByteArray &command) {
            // The server runs on this thread, so wait with the event loop going.
            QLocalSocket socket;
            socket.connectToServer(rig.dir.filePath("run/control.sock"));
            QByteArray reply;
            socket.write(command + "\n");
            if (!QTest::qWaitFor([&] { return (reply += socket.readAll()).endsWith('\n'); }, 3000))
                qWarning() << "no reply to" << command;
            return QJsonDocument::fromJson(reply).object();
        };
        QCOMPARE(ask("status").value("state").toString(), QString("idle"));
        QVERIFY(!ask("shot").value("ok").toBool());
        QCOMPARE(ask("toggle").value("state").toString(), QString("recording"));
        QVERIFY(ask("shot").value("ok").toBool());
        QSignalSpy shown(&control, &Control::showRequested);
        QVERIFY(ask("show").value("ok").toBool());
        QCOMPARE(shown.size(), 1);
        QVERIFY(!ask("explode").value("ok").toBool());
        QCOMPARE(ask("stop").value("state").toString(), QString("finishing"));
        QTRY_VERIFY(rig.backend.idle());
    }

    void barWidgetIsAValidPlugin() {
        const QString folder = QFINDTESTDATA("../share/omarchy-plugin/debrief-bar/manifest.json");
        const QJsonObject manifest = readJson(folder);
        QCOMPARE(manifest.value("id").toString(), QString("debrief.bar"));
        const QString entry = manifest.value("entryPoints").toObject().value("barWidget").toString();
        QVERIFY(QFile::exists(QFileInfo(folder).absolutePath() + "/" + entry));
        // An installed copy comes first; a run from build/ finds the source tree's.
        const QString found = Cli::pluginSource(QFileInfo(QFINDTESTDATA("../CMakeLists.txt")).absolutePath() + "/build");
        QVERIFY(QFile::exists(found + "/manifest.json"));
    }

    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QColor("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QColor("black"));
        QVERIFY(!theme.light());
        QCOMPARE(theme.radius(), 0);
    }

    void themeReadsThePalette() {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath("theme"));
        QVERIFY(write(dir.path() + "/theme/colors.toml", "mode = \"light\"\n"
                                                         "theme_type = \"dark\"\n"
                                                         "accent = \"#112233\"\n"
                                                         "bg = #fafafa # paper\n"
                                                         "foreground = '#202020'\n"
                                                         "color1 = \"#aa0000\"\n"));

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QColor("#112233"));
        QCOMPARE(theme.accentForeground(), QColor("white"));
        QCOMPARE(theme.background(), QColor("#fafafa"));
        QCOMPARE(theme.foreground(), QColor("#202020"));
        QCOMPARE(theme.colors().value("red").value<QColor>(), QColor("#aa0000"));
        QVERIFY(theme.light());
    }

    void themeAsksHyprlandAgainOnlyWhenItReloads() {
        // The fake Hyprland lives outside the theme directory, whose changes
        // the Theme also follows.
        QTemporaryDir dir, hypr;
        const QString command = fakeHyprctl(hypr.path());
        QVERIFY(write(hypr.filePath("response.json"), "{\"int\":4}"));
        QLocalServer events;
        QVERIFY(events.listen(hypr.filePath("events.sock")));

        // Once on start, and once more when the event socket connects.
        Theme theme(dir.path(), nullptr, command, events.fullServerName());
        QTRY_VERIFY(events.hasPendingConnections());
        QLocalSocket *hyprland = events.nextPendingConnection();
        QTRY_COMPARE(lines(hypr.filePath("calls")), 2);
        QTRY_COMPARE(theme.radius(), 4);

        QVERIFY(write(hypr.filePath("response.json"), "{\"int\":10}"));
        QTest::qWait(300);
        QCOMPARE(lines(hypr.filePath("calls")), 2);
        // Events arrive in pieces; only a whole configreloaded line counts.
        hyprland->write("activewindow>>kitty,~\nconfigrel");
        hyprland->flush();
        QTest::qWait(100);
        QCOMPARE(lines(hypr.filePath("calls")), 2);
        hyprland->write("oaded>>\n");
        QTRY_COMPARE(theme.radius(), 10);
        QCOMPARE(lines(hypr.filePath("calls")), 3);
    }

    void themeReachesHyprlandOnceItListens() {
        QTemporaryDir dir, hypr;
        const QString command = fakeHyprctl(hypr.path());
        QVERIFY(write(hypr.filePath("response.json"), "{\"int\":4}"));
        Theme theme(dir.path(), nullptr, command, hypr.filePath("events.sock"));
        QTRY_COMPARE(theme.radius(), 4);

        QVERIFY(write(hypr.filePath("response.json"), "{\"int\":8}"));
        QLocalServer events;
        QVERIFY(events.listen(hypr.filePath("events.sock")));
        QTRY_VERIFY_WITH_TIMEOUT(events.hasPendingConnections(), 5000);
        QTRY_COMPARE(theme.radius(), 8);
    }

    void translatorFallsBackToEnglish() {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile() {
        QTemporaryDir dir;
        QVERIFY(write(dir.path() + "/language", "pt-BR\n"));
        QVERIFY(write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})"));
        QVERIFY(write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})"));

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorSeesAFirstPick() {
        // Before the first pick there is no ~/.config/omarchy to watch.
        QTemporaryDir dir;
        QVERIFY(write(dir.path() + "/da.json", R"({"Save": "Gem"})"));
        Translator translator(dir.path() + "/omarchy/language", dir.path());
        QVERIFY(QDir(dir.path()).mkpath("omarchy"));
        QVERIFY(write(dir.path() + "/omarchy/language", "da\n"));
        QTRY_COMPARE(translator.translate("Main", "Save"), QString("Gem"));
    }

    void translatorPicksPluralForms() {
        QTemporaryDir dir;
        QVERIFY(write(dir.path() + "/language", "ru\n"));
        QVERIFY(write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"},
                                                   "%n files": {"one": "%n file", "other": "%n files"}})"));
        QVERIFY(write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})"));

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        // An untranslated counted string falls back to English, with English rules.
        QCOMPARE(translator.translate("Main", "%n files", nullptr, 21), QString("%n files"));
    }

    void englishCatalogListsEveryString() {
        // en.json is what translators work from: every text the sources
        // translate is in it, as itself or, when counted, as its English
        // plural forms, and nothing else is.
        const QString sources = QFileInfo(QFINDTESTDATA("../src/Main.qml")).absolutePath();
        QStringList problems;
        const QMap<QString, bool> used = translatables(sources, &problems);
        const QJsonObject english = readCatalog(sources + "/i18n/en.json");
        for (auto it = used.begin(); it != used.end(); ++it) {
            const QJsonValue value = english.value(it.key());
            if (value.isUndefined())
                problems << "missing from en.json: " + it.key();
            else if (it.value() && (value.toObject().keys() != QStringList{"one", "other"}
                                    || value.toObject().value("other").toString() != it.key()))
                problems << "counted, so needs its one and other forms: " + it.key();
            else if (!it.value() && value.toString() != it.key())
                problems << "should be itself: " + it.key();
        }
        for (const QString &key : english.keys())
            if (!used.contains(key))
                problems << "in en.json but no source translates it: " + key;
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join('\n')));
    }

    void catalogsMatchTheEnglishOne() {
        // A catalog may lack a string (it shows in English meanwhile), but what
        // it has translates something in en.json, with the same placeholders,
        // and a counted string has every form its language uses.
        const QString folder = QFileInfo(QFINDTESTDATA("../src/i18n/en.json")).absolutePath();
        const QJsonObject english = readCatalog(folder + "/en.json");
        QStringList problems;
        for (const QFileInfo &file : QDir(folder).entryInfoList({"*.json"}, QDir::Files)) {
            const QString code = file.completeBaseName();
            const QJsonObject catalog = readCatalog(file.filePath());
            if (catalog.isEmpty())
                problems << code + ": not a JSON object, or empty";
            // Some languages reach a form only at a million ("many" in French).
            QSet<QString> categories{"other", Translator::pluralCategory(code, 1000000)};
            for (long long n = 0; n <= 1000; ++n)
                categories << Translator::pluralCategory(code, n);
            for (auto it = catalog.begin(); it != catalog.end(); ++it) {
                const QString where = code + ": " + it.key() + ": ";
                const QJsonValue source = english.value(it.key());
                if (source.isUndefined()) {
                    problems << where + "not in en.json";
                } else if (source.isString()) {
                    if (!it.value().isString() || it.value().toString().isEmpty())
                        problems << where + "needs a translation";
                    else if (placeholders(it.value().toString()) != placeholders(it.key()))
                        problems << where + "has other placeholders than the English";
                } else {
                    const QJsonObject forms = it.value().toObject();
                    const QStringList wanted = placeholders(source.toObject().value("other").toString());
                    for (const QString &category : categories)
                        if (forms.value(category).toString().isEmpty())
                            problems << where + "needs its " + category + " form";
                    // A form other than "other" may say its number in words.
                    for (auto form = forms.begin(); form != forms.end(); ++form) {
                        const QStringList has = placeholders(form.value().toString());
                        QStringList needed = wanted;
                        if (form.key() != "other")
                            needed.removeIf([&](const QString &p) { return p.endsWith('n') && !has.contains(p); });
                        if (has != needed)
                            problems << where + form.key() + " has other placeholders than the English";
                    }
                }
            }
        }
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join('\n')));
    }

    void windowRecordsFromTheKeyboard() {
        // A temp language and theme, so the test never depends on the desktop's.
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language");
        QCoreApplication::installTranslator(&translator);
        Theme theme(dir.path());
        Rig rig;
        rig.transcriber->words = {"screenshot this"};

        QQmlApplicationEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError> &errors) {
            for (const auto &error : errors)
                warnings << error.toString();
        });
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("backend", &rig.backend);
        engine.rootContext()->setContextProperty("startHidden", false);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));

        auto *status = window->findChild<QObject *>("status");
        QCOMPARE(status->property("text").toString(), QString("Ready"));
        QTest::keyClick(window, Qt::Key_Space);
        QVERIFY(rig.backend.recording());
        QCOMPARE(status->property("text").toString(), QString("Recording 00:00:00"));

        rig.audio->push(quiet(1) + tone(1) + quiet(1.5));
        QCOMPARE(status->property("text").toString(), QString("Recording 00:00:03"));
        QTRY_COMPARE(window->findChild<QObject *>("shotCount")->property("text").toString(),
                     QString("1 screenshot"));
        auto *transcript = window->findChild<QObject *>("transcript");
        QTRY_COMPARE(transcript->property("count").toInt(), 2);

        // Quitting while recording asks first, and Escape keeps recording.
        auto *quit = window->findChild<QObject *>("quitDialog");
        QTest::keyClick(window, Qt::Key_Q);
        QTRY_VERIFY(quit->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!quit->property("visible").toBool());
        QVERIFY(rig.backend.recording());

        auto *help = window->findChild<QObject *>("help");
        QTest::keyClick(window, '?');
        QTRY_VERIFY(help->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!help->property("visible").toBool());

        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(rig.backend.idle());
        QVERIFY(window->findChild<QObject *>("report")->property("enabled").toBool());

        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        QCoreApplication::removeTranslator(&translator);
    }
};

QTEST_MAIN(DebriefTests)
#include "debrief_tests.moc"
