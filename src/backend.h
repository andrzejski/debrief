#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
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
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString notice READ notice CONSTANT)

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
    QString error() const { return m_error; }
    QString notice() const { return m_notice; }
    const SessionLog &log() const { return m_log; }

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

    // What `debrief status --json` reports.
    QJsonObject status() const;
    static QString defaultStateFile();
    static QString reportPrompt();

signals:
    void stateChanged();
    void elapsedChanged();
    void speakingChanged();
    void folderChanged();
    void entriesChanged();
    void errorChanged();
    void finished(const QString &folder);

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

    AudioSource *m_audio;
    Screenshotter *m_screenshots;
    Transcriber *m_transcriber;
    Namer *m_namer;
    QString m_root;
    QString m_stateFile;
    QString m_notice;

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
    bool m_notify = true;

    QHash<int, Capture> m_captures;
    int m_nextManual = 1000000;
};
