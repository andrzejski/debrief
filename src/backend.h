#pragma once

#include <QDateTime>
#include <QHash>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include "segmenter.h"
#include "services.h"
#include "session.h"

// A debugging session: records the microphone, transcribes what is said a
// sentence at a time, and keeps a screenshot of the screen whenever the words
// say "screenshot". The transcript and screenshots land in one folder per
// session, named for when it started and what it was about, with the
// recording beside it under the same name.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY stateChanged)
    Q_PROPERTY(bool idle READ idle NOTIFY stateChanged)
    Q_PROPERTY(int elapsed READ elapsed NOTIFY elapsedChanged)
    Q_PROPERTY(bool speaking READ speaking NOTIFY speakingChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY folderChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(int shotCount READ shotCount NOTIFY entriesChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY entriesChanged)
    Q_PROPERTY(bool naming READ naming NOTIFY entriesChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString notice READ notice CONSTANT)
    Q_PROPERTY(bool sop READ sop WRITE setSop NOTIFY sopChanged)

public:
    // The session takes ownership of the services. Tests pass fakes, a temp
    // root, and a temp state file.
    Backend(AudioSource *audio, Screenshotter *screenshots, Transcriber *transcriber, Namer *namer,
            const QString &root, const QString &stateFile, QObject *parent = nullptr);
    // The real microphone, screen, Superwhisper, and Claude.
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    QString state() const;
    bool recording() const { return m_state == Recording; }
    bool idle() const { return m_state == Idle; }
    int elapsed() const { return int(m_samples / kRate); }
    bool speaking() const { return m_speaking; }
    QString folder() const { return m_folder; }
    QVariantList entries() const;
    int shotCount() const { return m_log.shots.size(); }
    int pendingCount() const;
    bool naming() const { return m_naming; }
    QString error() const { return m_error; }
    QString notice() const { return m_notice; }
    const SessionLog &log() const { return m_log; }
    // What Report with Claude asks for: a QA report on what was reviewed
    // (the default), or a standard operating procedure for what was walked
    // through. The choice is remembered between runs.
    bool sop() const { return m_sop; }
    void setSop(bool sop);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void screenshotNow();
    Q_INVOKABLE void openFolder();
    Q_INVOKABLE void openRoot();
    Q_INVOKABLE void reportWithClaude();
    Q_INVOKABLE void dismissError();
    // A desktop notification when a session is saved; tests turn it off.
    void setNotify(bool notify) { m_notify = notify; }
    // Showing the finished report in the file manager; tests turn it off.
    void setRevealReports(bool reveal) { m_revealReports = reveal; }
    // Watches a session folder until Claude has written both reports, then
    // shows the folder with the page selected and emits reportReady.
    void watchForReport(const QString &folder, bool sop = false);
    // Whether Claude Code has been let into a folder: its trust flag for the
    // folder in .claude.json, or a conversation started there since `since`.
    // `home` is the user's home folder when empty; tests pass their own.
    static bool folderTrusted(const QString &folder, const QDateTime &since, const QString &home = {});

    // What `debrief status --json` reports.
    QJsonObject status() const;
    static QString defaultStateFile();
    static QString reportPrompt(bool sop = false);
    // The two files the report is made of, in the session folder, and the
    // two a standard operating procedure is made of.
    static inline const QString kReportMarkdown = QStringLiteral("Debrief-Report.md");
    static inline const QString kReportPage = QStringLiteral("Debrief-Report.html");
    static inline const QString kSopMarkdown = QStringLiteral("Debrief-SOP.md");
    static inline const QString kSopPage = QStringLiteral("Debrief-SOP.html");
    // The Claude Code skill and the report page it starts from, copied out of
    // the program into a skills folder.
    static QString defaultSkillFolder();
    static bool installSkill(const QString &folder = defaultSkillFolder());

signals:
    void stateChanged();
    void elapsedChanged();
    void speakingChanged();
    void folderChanged();
    void entriesChanged();
    void errorChanged();
    void finished(const QString &folder);
    void reportReady(const QString &folder);
    void sopChanged();

private:
    enum State { Idle, Recording, Finishing };
    static constexpr int kRate = 16000;

    struct Capture {
        int speech = -1;     // the utterance it belongs to
        QString path;        // where grim writes it
        qint64 timeMs = 0;
        bool manual = false;
        bool done = false;
        bool ok = false;
        QString error;
    };

    void setState(State state);
    void setError(const QString &error);
    void onAudio(const QByteArray &pcm);
    void onUtterance(const Segmenter::Utterance &utterance);
    void onTranscribed(int id, bool ok, const QString &text, const QString &error);
    void onCaptured(int id, bool ok, const QString &error);
    void onAudioStopped();
    void resolve(int captureId);
    void keepShot(const Capture &capture, const QString &source);
    void finishIfDone();
    void onNamed(const QString &words);
    QStringList said() const;
    void save();
    void writeState() const;
    QString shotName(int number, qint64 timeMs) const;
    void checkReport();
    void revealReport(const QString &folder, const QString &page);
    void watchTerminalFocus(const QString &folder, quintptr before);
    void checkTerminalFocus();

    AudioSource *m_audio;
    Screenshotter *m_screenshots;
    Transcriber *m_transcriber;
    Namer *m_namer;
    QString m_root;
    QString m_stateFile;
    QString m_settingsFile;
    QString m_notice;
    bool m_sop = false;

    State m_state = Idle;
    QString m_folder;
    QString m_error;
    SessionLog m_log;
    Debrief::WavWriter m_wav;
    Segmenter m_segmenter;
    QByteArray m_buffer;      // the most recent audio, for cutting utterances
    qint64 m_bufferStart = 0; // sample index of m_buffer's first sample
    qint64 m_samples = 0;
    bool m_speaking = false;
    bool m_audioStopped = true;
    bool m_naming = false;
    QElapsedTimer m_finishTimer;
    bool m_notify = true;

    // The report Claude is writing: checked every second until the page is
    // newer than the request and has stopped changing, for up to three hours.
    QTimer m_reportPoll;
    QString m_reportFolder;
    QString m_reportMarkdown; // the two files being waited for
    QString m_reportPage;
    QDateTime m_reportAsked;
    QDateTime m_reportSeen;   // the page's last-modified time at the previous check
    qint64 m_reportSeenSize = -1;
    QElapsedTimer m_reportWaited;
    bool m_revealReports = true;

    // The terminal Claude opened in: once the folder is trusted there is
    // nothing more to answer, so focus goes back to the window that had it
    // before. Windows only; given up after ten minutes.
    QTimer m_focusPoll;
    QString m_focusFolder;
    QDateTime m_focusAsked;
    QElapsedTimer m_focusWaited;
    quintptr m_focusBefore = 0;   // the window in front when the report was asked for
    quintptr m_focusTerminal = 0; // the terminal's window, once it has come to the front

    QHash<int, Capture> m_captures;
    int m_nextManual = 1000000;
};
