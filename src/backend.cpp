#include "backend.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QThread>
#include <QUrl>
#include <algorithm>

#ifdef DEBRIEF_NATIVE
#include "native/native_services.h"
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
// Windows refuses to rename a file or folder while another program has
// something in it open, and virus scanning or search indexing opens what the
// session has just written for a moment. So try again for up to a second
// before settling for the name it already has.
bool renameSoon(const QString &from, const QString &to) {
    for (int attempt = 0;; ++attempt) {
        if (QFileInfo(from).isDir() ? QDir().rename(from, to) : QFile::rename(from, to))
            return true;
#ifdef Q_OS_WIN
        if (attempt < 20 && QFileInfo::exists(from) && !QFileInfo::exists(to)) {
            QThread::msleep(50);
            continue;
        }
#endif
        qWarning("Couldn't rename %s to %s", qPrintable(from), qPrintable(to));
        return false;
    }
}
} // namespace

Backend::Backend(AudioSource *audio, Screenshotter *screenshots, Transcriber *transcriber, Namer *namer,
                 const QString &root, const QString &stateFile, QObject *parent)
    : QObject(parent), m_audio(audio), m_screenshots(screenshots), m_transcriber(transcriber), m_namer(namer),
      m_root(root.isEmpty() ? Debrief::defaultRoot() : root),
      m_stateFile(stateFile.isEmpty() ? defaultStateFile() : stateFile), m_segmenter(kRate) {
    for (QObject *service : {static_cast<QObject *>(audio), static_cast<QObject *>(screenshots),
                             static_cast<QObject *>(transcriber), static_cast<QObject *>(namer)})
        service->setParent(this);
    m_notice = m_transcriber->unavailableReason();
    // Beside the sessions when a root was given (tests), otherwise with the
    // rest of the app's settings.
    m_settingsFile = root.isEmpty()
                         ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/settings.ini"
                         : m_root + "/settings.ini";
    m_sop = QSettings(m_settingsFile, QSettings::IniFormat).value("report/kind").toString() == "sop";

    connect(m_audio, &AudioSource::audio, this, &Backend::onAudio);
    connect(m_audio, &AudioSource::stopped, this, &Backend::onAudioStopped);
    connect(m_audio, &AudioSource::failed, this, [this](const QString &message) {
        setError(message);
        // What was recorded up to here is still saved.
        if (m_state == Recording) {
            setState(Finishing);
            onAudioStopped();
        }
    });
    connect(m_screenshots, &Screenshotter::captured, this, &Backend::onCaptured);
    connect(m_transcriber, &Transcriber::transcribed, this, &Backend::onTranscribed);
    connect(m_namer, &Namer::named, this, &Backend::onNamed);
    m_reportPoll.setInterval(1000);
    m_focusPoll.setInterval(500);
    connect(&m_focusPoll, &QTimer::timeout, this, &Backend::checkTerminalFocus);
    connect(&m_reportPoll, &QTimer::timeout, this, &Backend::checkReport);
    writeState();
}

#ifdef DEBRIEF_NATIVE
// Nothing beside the app: Qt for the microphone and the screen, whisper.cpp inside.
Backend::Backend(QObject *parent)
    : Backend(new QtAudioSource, new QtScreenshotter, new WhisperTranscriber, new ClaudeNamer, {}, {}, parent) {}
#else
Backend::Backend(QObject *parent)
    : Backend(new FfmpegAudioSource, new GrimScreenshotter, new SuperwhisperTranscriber, new ClaudeNamer, {}, {}, parent) {}
#endif

Backend::~Backend() {
    // The bar should not show a session that went with the app.
    m_state = Idle;
    writeState();
}

QString Backend::defaultStateFile() {
    QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty())
        runtime = QDir::tempPath();
    return runtime + "/debrief/state.json";
}

void Backend::setSop(bool sop) {
    if (m_sop == sop)
        return;
    m_sop = sop;
    QDir().mkpath(QFileInfo(m_settingsFile).absolutePath());
    QSettings(m_settingsFile, QSettings::IniFormat).setValue("report/kind", sop ? "sop" : "qa");
    emit sopChanged();
}

QString Backend::reportPrompt(bool sop) {
    // One line with no double quotes: on Windows it travels inside a quoted
    // argument in a .cmd script. The skill has the detail; this says enough
    // to get the same report where the skill didn't load.
    if (sop)
        return QStringLiteral(
            "This folder is a Debrief session in which I walked through how to do something. transcript.md is what "
            "I said, with each screenshot placed under the words I said when I took it; session.json has the same "
            "links (screenshots[].transcript_index, said, and context). Read both and look at every screenshot (the "
            "shot-*.png files here). Then write a standard operating procedure for the process I walked through, as "
            "two files here, following the SOP part of the debrief skill. First Debrief-SOP.md: a title naming the "
            "procedure, a short purpose and scope, what is needed before starting, and then the numbered steps in the "
            "order I did them, each written as an instruction, naming the screen, button, or field by its own label, "
            "with its screenshot embedded and its time code. Second Debrief-SOP.html: a self-contained page with the "
            "same procedure, an at-a-glance list of the steps at the top, and under every step crops of just the "
            "part of the screenshot that step is about, saved in a crops folder and linked to the full screenshot. "
            "Start the page from sop-template.html in the skill folder (~/.claude/skills/debrief) if it is there. "
            "Include the tips, warnings, and checks I mentioned at the step they belong to, say where the transcript "
            "is garbled and how you read it, mark any step the session leaves unclear as needing confirmation, and "
            "don't invent steps the session doesn't show.");
    return QStringLiteral(
        "This folder is a Debrief session. transcript.md is what I said, with each screenshot placed under the "
        "words I said when I took it; session.json has the same links (screenshots[].transcript_index, said, and "
        "context). Read both and look at every screenshot (the shot-*.png files here). Then write two reports "
        "here, following the debrief skill. First Debrief-Report.md: a one-paragraph summary, steps to reproduce, "
        "expected and actual behaviour, and then each thing I raised in order, with its screenshot embedded, its "
        "time code, and what I said about it. Second Debrief-Report.html: a self-contained page with the same feedback "
        "regrouped by the part of the product it is about (each screen, feature, or document I reviewed), a "
        "scorecard at the top with my verdict on each, and under every entry my quote, its time code, and crops "
        "of just the part of the screenshot that entry is about, saved in a crops folder and linked to the full "
        "screenshot. Start the page from report-template.html in the skill folder (~/.claude/skills/debrief) if "
        "it is there. Mark what worked well as well as what is broken, say where the transcript is garbled and "
        "how you read it, quote me where it helps, and don't invent details the session doesn't show.");
}

QString Backend::defaultSkillFolder() {
    return QDir::homePath() + "/.claude/skills/debrief";
}

bool Backend::installSkill(const QString &folder) {
    if (!QDir().mkpath(folder))
        return false;
    for (const char *name : {"SKILL.md", "report-template.html", "sop-template.html"}) {
        QFile source(QStringLiteral(":/skill/") + name);
        if (!source.open(QIODevice::ReadOnly))
            return false;
        const QByteArray contents = source.readAll();
        QFile target(folder + "/" + name);
        if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate) || target.write(contents) != contents.size())
            return false;
    }
    return true;
}

QString Backend::state() const {
    switch (m_state) {
    case Idle: return "idle";
    case Recording: return "recording";
    case Finishing: return "finishing";
    }
    return {};
}

int Backend::pendingCount() const {
    int pending = int(std::count_if(m_log.speech.begin(), m_log.speech.end(),
                                    [](const Speech &s) { return s.state == Speech::Pending; }));
    for (const Capture &capture : m_captures)
        if (capture.manual && !capture.done)
            ++pending;
    return pending;
}

void Backend::setState(State state) {
    if (m_state == state)
        return;
    m_state = state;
    writeState();
    emit stateChanged();
}

void Backend::setError(const QString &error) {
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}

void Backend::dismissError() { setError({}); }

void Backend::toggle() {
    if (m_state == Idle)
        start();
    else if (m_state == Recording)
        stop();
}

void Backend::start() {
    if (m_state != Idle)
        return;
    const QDateTime now = QDateTime::currentDateTime();
    // Named for when it started; what it was about is added once that is known.
    QString folder = m_root + "/" + now.toString("yyyy-MM-dd_HH-mm-ss");
    for (int n = 2; QFileInfo::exists(folder) || QFileInfo::exists(folder + ".wav"); ++n)
        folder = m_root + "/" + now.toString("yyyy-MM-dd_HH-mm-ss") + QString("_%1").arg(n);
    if (!QDir().mkpath(folder + "/.work")) {
        setError(tr("Couldn't create the session folder %1.").arg(folder));
        return;
    }
    // The recording sits beside the folder, so the folder alone goes to Claude.
    if (!m_wav.open(folder + ".wav")) {
        setError(tr("Couldn't write the recording: %1").arg(m_wav.errorString()));
        return;
    }

    setError({});
    m_folder = folder;
    m_log = SessionLog();
    m_log.started = now;
    m_log.audio = "../" + QFileInfo(folder).fileName() + ".wav";
    m_naming = false;
    m_segmenter = Segmenter(kRate);
    m_buffer.clear();
    m_bufferStart = 0;
    m_samples = 0;
    m_captures.clear();
    m_speaking = false;
    m_audioStopped = false;
    emit folderChanged();
    emit elapsedChanged();
    emit speakingChanged();
    emit entriesChanged();
    setState(Recording);
    save();
    m_audio->start();
}

void Backend::stop() {
    if (m_state != Recording)
        return;
    m_finishTimer.start();
    qInfo("Session stop requested; pending transcripts: %d", pendingCount());
    setState(Finishing);
    m_audio->stop();
}

void Backend::onAudio(const QByteArray &pcm) {
    // stop() changes state before the audio source drains its final buffer.
    // Keep that tail until stopped(), but reject any late audio afterwards.
    if (m_state != Recording && !(m_state == Finishing && !m_audioStopped))
        return;
    if (!m_wav.append(pcm))
        setError(tr("Couldn't write the recording: %1").arg(m_wav.errorString()));
    m_buffer.append(pcm);
    const int before = elapsed();
    m_samples += pcm.size() / 2;

    const auto ended = m_segmenter.feed(reinterpret_cast<const qint16 *>(pcm.constData()), pcm.size() / 2);
    for (const auto &utterance : ended)
        onUtterance(utterance);

    // Keep only what the longest utterance and its padding can still need.
    const qint64 keep = m_segmenter.maxLength() + kRate * 5;
    const qint64 have = m_buffer.size() / 2;
    if (have > keep * 2) {
        const qint64 drop = have - keep;
        m_buffer.remove(0, drop * 2);
        m_bufferStart += drop;
    }

    if (m_segmenter.inSpeech() != m_speaking) {
        m_speaking = m_segmenter.inSpeech();
        emit speakingChanged();
    }
    if (elapsed() != before)
        emit elapsedChanged();
}

void Backend::onUtterance(const Segmenter::Utterance &utterance) {
    const int index = m_log.speech.size();
    Speech speech;
    speech.startMs = utterance.start * 1000 / kRate;
    speech.endMs = utterance.end * 1000 / kRate;
    m_log.speech.append(speech);

    const qint64 from = std::max<qint64>(0, utterance.start - m_bufferStart);
    const qint64 to = std::min<qint64>(m_buffer.size() / 2, utterance.end - m_bufferStart);
    const QString chunk = QString("%1/.work/speech-%2.wav").arg(m_folder).arg(index, 4, 10, QChar('0'));
    if (to <= from || !Debrief::WavWriter::write(chunk, m_buffer.mid(from * 2, (to - from) * 2), kRate)) {
        m_log.speech[index].state = Speech::Failed;
        m_log.speech[index].error = "couldn't cut the audio";
    } else if (m_notice.isEmpty()) {
        // The screen is captured the moment the words end, which is when
        // "screenshot" was just said, and kept only if the words turn out to
        // have it in them.
        Capture capture;
        capture.speech = index;
        capture.timeMs = speech.endMs;
        capture.path = QString("%1/.work/candidate-%2.png").arg(m_folder).arg(index, 4, 10, QChar('0'));
        m_captures.insert(index, capture);
        m_screenshots->capture(index, capture.path);
        m_transcriber->transcribe(index, chunk);
    } else {
        m_log.speech[index].state = Speech::Failed;
        m_log.speech[index].error = "no transcriber";
        QFile::remove(chunk);
    }
    m_wav.updateHeader();
    m_log.durationMs = m_samples * 1000 / kRate;
    save();
    emit entriesChanged();
}

void Backend::onTranscribed(int id, bool ok, const QString &text, const QString &error) {
    if (id < 0 || id >= m_log.speech.size() || m_folder.isEmpty())
        return;
    Speech &speech = m_log.speech[id];
    speech.state = ok ? Speech::Done : Speech::Failed;
    speech.text = text;
    speech.error = error;
    QFile::remove(QString("%1/.work/speech-%2.wav").arg(m_folder).arg(id, 4, 10, QChar('0')));
    resolve(id);
    save();
    emit entriesChanged();
    finishIfDone();
}

void Backend::onCaptured(int id, bool ok, const QString &error) {
    auto it = m_captures.find(id);
    if (it == m_captures.end())
        return;
    it->done = true;
    it->ok = ok;
    it->error = error;
    if (it->manual) {
        const Capture capture = *it;
        m_captures.erase(it);
        if (ok)
            keepShot(capture, "manual");
        else
            setError(tr("Couldn't capture the screen. %1").arg(error));
        save();
        emit entriesChanged();
        finishIfDone();
        return;
    }
    resolve(id);
    save();
    emit entriesChanged();
    finishIfDone();
}

// Decides a candidate screenshot once both its capture and its words are in.
void Backend::resolve(int id) {
    auto it = m_captures.find(id);
    if (it == m_captures.end() || it->manual || !it->done || id >= m_log.speech.size()
        || m_log.speech[id].state == Speech::Pending)
        return;
    const Capture capture = *it;
    m_captures.erase(it);
    const Speech &speech = m_log.speech[id];
    const bool wanted = speech.state == Speech::Done && Debrief::asksForScreenshot(speech.text);
    if (wanted && capture.ok)
        keepShot(capture, "voice");
    else
        QFile::remove(capture.path);
    if (wanted && !capture.ok)
        setError(tr("Couldn't capture the screen. %1").arg(capture.error));
}

void Backend::keepShot(const Capture &capture, const QString &source) {
    Shot shot;
    shot.number = m_log.shots.size() + 1;
    shot.timeMs = capture.timeMs;
    shot.source = source;
    shot.speech = capture.speech;
    shot.file = shotName(shot.number, shot.timeMs);
    const QString target = m_folder + "/" + shot.file;
    if (capture.path != target && !QFile::rename(capture.path, target)) {
        setError(tr("Couldn't save screenshot %1.").arg(shot.number));
        return;
    }
    m_log.shots.append(shot);
    writeState();
}

QString Backend::shotName(int number, qint64 timeMs) const {
    return QString("shot-%1-%2.png")
        .arg(number, 2, 10, QChar('0'))
        .arg(Debrief::timecode(timeMs).replace(':', '-'));
}

void Backend::screenshotNow() {
    if (m_state != Recording)
        return;
    Capture capture;
    capture.manual = true;
    capture.timeMs = m_samples * 1000 / kRate;
    // It belongs to the words being said, or else to the last ones.
    capture.speech = m_segmenter.inSpeech() ? m_log.speech.size() : m_log.speech.size() - 1;
    const int id = m_nextManual++;
    // A temporary name: the number is given when it lands, in order.
    capture.path = QString("%1/.work/manual-%2.png").arg(m_folder).arg(id);
    m_captures.insert(id, capture);
    emit entriesChanged();
    m_screenshots->capture(id, capture.path);
}

void Backend::onAudioStopped() {
    if (m_state != Finishing)
        return;
    m_audioStopped = true;
    qInfo("Audio stopped after %lld ms", m_finishTimer.isValid() ? m_finishTimer.elapsed() : -1);
    if (const auto last = m_segmenter.flush())
        onUtterance(*last);
    if (m_speaking) {
        m_speaking = false;
        emit speakingChanged();
    }
    m_log.durationMs = m_samples * 1000 / kRate;
    m_wav.close();
    finishIfDone();
}

void Backend::finishIfDone() {
    if (m_state != Finishing || !m_audioStopped || pendingCount() > 0 || !m_captures.isEmpty() || m_naming)
        return;
    QDir(m_folder + "/.work").removeRecursively();
    // The last step: what the session was about, for the end of its name.
    m_naming = true;
    qInfo("Transcription/captures complete after %lld ms; naming session",
          m_finishTimer.isValid() ? m_finishTimer.elapsed() : -1);
    emit entriesChanged();
    const QStringList words = said();
    if (words.isEmpty())
        onNamed({});
    else
        m_namer->name(words.join('\n'));
}

QStringList Backend::said() const {
    QStringList words;
    for (const Speech &speech : m_log.speech)
        if (speech.state == Speech::Done && !speech.text.isEmpty())
            words << speech.text;
    return words;
}

void Backend::onNamed(const QString &words) {
    if (!m_naming || m_state != Finishing)
        return;
    m_naming = false;
    QString about = Debrief::slug(words);
    if (about.isEmpty())
        about = Debrief::describe(said());

    QString named = m_folder + "_" + about;
    for (int n = 2; QFileInfo::exists(named) || QFileInfo::exists(named + ".wav"); ++n)
        named = m_folder + "_" + about + QString("-%1").arg(n);
    // Both or neither, so the recording always carries its folder's name.
    if (renameSoon(m_folder, named)) {
        if (renameSoon(m_folder + ".wav", named + ".wav")) {
            m_folder = named;
            m_log.audio = "../" + QFileInfo(named).fileName() + ".wav";
        } else {
            renameSoon(named, m_folder);
        }
    }
    m_log.title = about.replace('-', ' ');
    m_log.finished = true;
    save();
    emit folderChanged();
    setState(Idle);
    emit entriesChanged();
    // On Windows and macOS the tray icon says it, from the finished signal.
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    if (m_notify)
        startDetachedQuietly("notify-send",
                            {"--app-name=Debrief", "--icon=debrief", tr("Debrief session saved"),
                             tr("%Ln screenshots, %1 of audio, in %2", "", m_log.shots.size())
                                 .arg(Debrief::timecode(m_log.durationMs), QDir::toNativeSeparators(m_folder))});
#endif
    qInfo("Session saved after %lld ms", m_finishTimer.isValid() ? m_finishTimer.elapsed() : -1);
    emit finished(m_folder);
}

void Backend::save() {
    if (m_folder.isEmpty())
        return;
    QString error;
    if (!m_log.save(m_folder, &error))
        setError(tr("Couldn't save the transcript: %1").arg(error));
}

void Backend::writeState() const {
    QDir().mkpath(QFileInfo(m_stateFile).absolutePath());
    QSaveFile file(m_stateFile);
    file.setDirectWriteFallback(false);
    const QByteArray json = QJsonDocument(status()).toJson(QJsonDocument::Compact) + "\n";
    if (!file.open(QIODevice::WriteOnly) || file.write(json) != json.size() || !file.commit())
        return;
    // The bar widget reads the file when told to.
    static const QString shell = QStandardPaths::findExecutable("omarchy-shell");
    if (m_notify && !shell.isEmpty() && m_stateFile == defaultStateFile())
        startDetachedQuietly(shell, {"debrief.bar", "refresh"});
}

QJsonObject Backend::status() const {
    return {{"running", true},
            {"state", state()},
            {"folder", m_folder},
            {"started", m_log.started.isValid() ? m_log.started.toString(Qt::ISODate) : QString()},
            {"elapsed_s", elapsed()},
            {"screenshots", int(m_log.shots.size())},
            {"pid", QCoreApplication::applicationPid()}};
}

QVariantList Backend::entries() const {
    struct Row { qint64 at; int order; QVariantMap map; };
    QList<Row> rows;
    for (int i = 0; i < m_log.speech.size(); ++i) {
        const Speech &s = m_log.speech[i];
        rows.append({s.startMs, 0, QVariantMap{{"kind", "speech"},
                                               {"time", Debrief::timecode(s.startMs)},
                                               {"text", s.text},
                                               {"state", s.state == Speech::Done ? "done"
                                                         : s.state == Speech::Pending ? "pending" : "failed"},
                                               {"error", s.error}}});
    }
    for (const Shot &shot : m_log.shots) {
        const qint64 at = shot.speech >= 0 && shot.speech < m_log.speech.size() ? m_log.speech[shot.speech].startMs
                                                                                 : shot.timeMs;
        rows.append({at, 1, QVariantMap{{"kind", "shot"},
                                        {"time", Debrief::timecode(shot.timeMs)},
                                        {"number", shot.number},
                                        {"source", shot.source},
                                        {"image", QUrl::fromLocalFile(m_folder + "/" + shot.file).toString()}}});
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        return a.at != b.at ? a.at < b.at : a.order < b.order;
    });
    QVariantList list;
    for (const Row &row : rows)
        list.append(row.map);
    return list;
}

void Backend::openFolder() {
    if (!m_folder.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_folder));
}

void Backend::openRoot() {
    QDir().mkpath(m_root);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_root));
}

void Backend::reportWithClaude() {
    if (m_folder.isEmpty() || m_state != Idle)
        return;
    // The skill says how the report is laid out and carries the page it
    // starts from, so it goes in with the app's own copy each time. Without
    // it the prompt still asks for the same report.
    installSkill();
#ifdef Q_OS_WIN
    // A script rather than a command line: cmd's quoting would have to
    // survive the prompt's punctuation. It opens a console of its own in the
    // session folder, and the console stays after Claude exits.
    auto literal = [](QString text) { return text.replace('%', "%%"); };
    QTemporaryFile script(QDir::tempPath() + "/debrief-report-XXXXXX.cmd");
    script.setAutoRemove(false);
    const QByteArray lines = ("@echo off\r\nchcp 65001 >nul\r\ncd /d \"" + literal(QDir::toNativeSeparators(m_folder))
                              + "\"\r\nclaude \"" + literal(reportPrompt(m_sop)) + "\"\r\n")
                                 .toUtf8();
    if (!script.open() || script.write(lines) != lines.size()) {
        setError(tr("Couldn't open Claude in a terminal."));
        return;
    }
    script.close();
    const quintptr before = quintptr(GetForegroundWindow());
    QProcess process;
    process.setProgram("cmd.exe");
    process.setArguments({"/k", QDir::toNativeSeparators(script.fileName())});
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
        arguments->flags |= CREATE_NEW_CONSOLE;
        arguments->flags &= ~DWORD(CREATE_NO_WINDOW);
        arguments->startupInfo->dwFlags &= ~DWORD(STARTF_USESTDHANDLES);
    });
    if (!process.startDetached()) {
        setError(tr("Couldn't open Claude in a terminal."));
        return;
    }
    watchForReport(m_folder, m_sop);
    watchTerminalFocus(m_folder, before);
    return;
#endif
#ifdef Q_OS_MACOS
    // Terminal runs a .command file in a window of its own. The script's
    // shell is a login, interactive one, so Claude Code is found wherever the
    // user's own shell setup puts it, and the window stays after Claude
    // exits. The prompt has nothing a double-quoted shell word would act on.
    QTemporaryFile script(QDir::tempPath() + "/debrief-report-XXXXXX.command");
    script.setAutoRemove(false);
    const QByteArray lines = ("#!/bin/zsh -il\ncd -- '" + QString(m_folder).replace("'", "'\\''")
                              + "' || exit 1\nclaude \"" + reportPrompt(m_sop) + "\"\nexec /bin/zsh -il\n")
                                 .toUtf8();
    if (!script.open() || script.write(lines) != lines.size()) {
        setError(tr("Couldn't open Claude in a terminal."));
        return;
    }
    script.close();
    QFile::setPermissions(script.fileName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    if (!startDetachedQuietly("/usr/bin/open", {"-a", "Terminal", script.fileName()})) {
        setError(tr("Couldn't open Claude in a terminal."));
        return;
    }
    watchForReport(m_folder, m_sop);
    return;
#endif
    const QString terminal = QStandardPaths::findExecutable("xdg-terminal-exec");
    if (terminal.isEmpty()) {
        setError(tr("xdg-terminal-exec isn't installed, so Claude can't be opened in a terminal."));
        return;
    }
    // An interactive shell, so Claude Code is found wherever the user's own
    // shell setup puts it, and the terminal stays open after Claude exits.
    if (startDetachedQuietly(terminal, {"bash", "-ic", "cd -- \"$1\" && claude \"$2\"; exec bash", "debrief",
                                        m_folder, reportPrompt(m_sop)}))
        watchForReport(m_folder, m_sop);
}

void Backend::watchForReport(const QString &folder, bool sop) {
    // One report at a time: asking again, here or for another session,
    // follows the newest request.
    m_reportFolder = folder;
    m_reportMarkdown = sop ? kSopMarkdown : kReportMarkdown;
    m_reportPage = sop ? kSopPage : kReportPage;
    m_reportAsked = QDateTime::currentDateTime();
    m_reportSeen = {};
    m_reportSeenSize = -1;
    m_reportWaited.start();
    m_reportPoll.start();
}

void Backend::checkReport() {
    constexpr qint64 kGiveUpMs = 3 * 60 * 60 * 1000;
    if (m_reportFolder.isEmpty() || m_reportWaited.elapsed() > kGiveUpMs) {
        m_reportPoll.stop();
        m_reportFolder.clear();
        return;
    }
    // The page is written last. One from an earlier report doesn't count, and
    // a page still being written waits until it is the same at two checks.
    const QFileInfo markdown(m_reportFolder + "/" + m_reportMarkdown);
    const QFileInfo page(m_reportFolder + "/" + m_reportPage);
    if (!markdown.exists() || !page.exists() || page.lastModified() < m_reportAsked.addSecs(-2))
        return;
    if (page.lastModified() != m_reportSeen || page.size() != m_reportSeenSize) {
        m_reportSeen = page.lastModified();
        m_reportSeenSize = page.size();
        return;
    }
    const QString folder = m_reportFolder;
    m_reportPoll.stop();
    m_reportFolder.clear();
    if (m_revealReports)
        revealReport(folder, m_reportPage);
    emit reportReady(folder);
}

void Backend::revealReport(const QString &folder, const QString &page) {
#ifdef Q_OS_WIN
    // Explorer opens on the session folder with the page selected. Its
    // /select switch takes the path quoted after the comma, which QProcess
    // can't produce from a plain argument list.
    QProcess process;
    process.setProgram("explorer.exe");
    process.setNativeArguments("/select,\"" + QDir::toNativeSeparators(folder + "/" + page) + "\"");
    if (process.startDetached())
        return;
#endif
#ifdef Q_OS_MACOS
    // Finder opens on the session folder with the page selected.
    if (startDetachedQuietly("/usr/bin/open", {"-R", folder + "/" + page}))
        return;
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

bool Backend::folderTrusted(const QString &folder, const QDateTime &since, const QString &home) {
    const QString base = home.isEmpty() ? QDir::homePath() : home;
    const QString wanted = QDir::cleanPath(QDir::fromNativeSeparators(folder));
    // Claude Code keeps the answer to its trust question per folder.
    QFile config(base + "/.claude.json");
    if (config.open(QIODevice::ReadOnly)) {
        const QJsonObject projects = QJsonDocument::fromJson(config.readAll()).object().value("projects").toObject();
        for (auto it = projects.begin(); it != projects.end(); ++it) {
            if (QDir::cleanPath(QDir::fromNativeSeparators(it.key())).compare(wanted, Qt::CaseInsensitive) == 0
                && it.value().toObject().value("hasTrustDialogAccepted").toBool())
                return true;
        }
    }
    // A folder under one already trusted is never asked about, so a
    // conversation started there counts too. Claude Code files them under the
    // folder's path with everything but letters and digits turned to dashes.
    static const QRegularExpression notPlain("[^A-Za-z0-9]");
    const QDir conversations(base + "/.claude/projects/" + QString(wanted).replace(notPlain, "-"));
    const QFileInfoList started = conversations.entryInfoList({"*.jsonl"}, QDir::Files);
    return std::any_of(started.begin(), started.end(),
                       [&](const QFileInfo &file) { return file.lastModified() >= since.addSecs(-2); });
}

void Backend::watchTerminalFocus(const QString &folder, quintptr before) {
    m_focusFolder = folder;
    m_focusAsked = QDateTime::currentDateTime();
    m_focusBefore = before;
    m_focusTerminal = 0;
    m_focusWaited.start();
    m_focusPoll.start();
}

void Backend::checkTerminalFocus() {
#ifdef Q_OS_WIN
    constexpr qint64 kGiveUpMs = 10 * 60 * 1000;
    if (m_focusFolder.isEmpty() || m_focusWaited.elapsed() > kGiveUpMs) {
        m_focusPoll.stop();
        return;
    }
    const HWND front = GetForegroundWindow();
    if (!m_focusTerminal && front && quintptr(front) != m_focusBefore) {
        // The terminal is the console or Windows Terminal window that comes
        // to the front after the launch; anything else in front is the user
        // having moved on already.
        DWORD owner = 0;
        GetWindowThreadProcessId(front, &owner);
        wchar_t name[64] = {};
        GetClassNameW(front, name, 64);
        const QString type = QString::fromWCharArray(name);
        if (owner != GetCurrentProcessId()
            && (type == "ConsoleWindowClass" || type == "CASCADIA_HOSTING_WINDOW_CLASS"))
            m_focusTerminal = quintptr(front);
    }
    if (!m_focusTerminal || !folderTrusted(m_focusFolder, m_focusAsked))
        return;
    m_focusPoll.stop();
    m_focusFolder.clear();
    const HWND terminal = HWND(m_focusTerminal);
    // Only take focus from the terminal, never from wherever the user went.
    if (front != terminal)
        return;
    // Back to the window that had focus before, or, if that has gone, the
    // one just under the terminal, which is the one used most recently.
    auto usable = [](HWND window) {
        return window && IsWindow(window) && IsWindowVisible(window) && !IsIconic(window)
               && GetWindowTextLengthW(window) > 0 && !(GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW);
    };
    HWND target = HWND(m_focusBefore);
    if (!usable(target) || target == terminal) {
        target = GetWindow(terminal, GW_HWNDNEXT);
        while (target && !usable(target))
            target = GetWindow(target, GW_HWNDNEXT);
    }
    if (!target)
        return;
    // Windows only lets the thread with the foreground window hand it on, so
    // borrow the terminal's input state for the call.
    const DWORD theirs = GetWindowThreadProcessId(terminal, nullptr);
    const DWORD ours = GetCurrentThreadId();
    const bool joined = theirs != ours && AttachThreadInput(ours, theirs, TRUE);
    SetForegroundWindow(target);
    if (joined)
        AttachThreadInput(ours, theirs, FALSE);
#else
    m_focusPoll.stop();
#endif
}
