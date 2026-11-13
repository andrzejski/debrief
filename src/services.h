#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QTimer>

// Starts a program that outlives this one, with its output going nowhere, so
// it never holds on to the terminal or pipe this one was started from.
bool startDetachedQuietly(const QString &program, const QStringList &arguments);

// The three things a session needs from the desktop, each behind an interface
// so the tests drive a session with fakes and no microphone, screen, or
// Superwhisper.

// 16 kHz mono 16-bit audio from the default microphone.
class AudioSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void start() = 0;
    virtual void stop() = 0;
signals:
    void audio(const QByteArray &pcm);
    void failed(const QString &message);
    void stopped();
};

// A screenshot of every screen, written to a PNG.
class Screenshotter : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void capture(int id, const QString &path) = 0;
signals:
    void captured(int id, bool ok, const QString &error);
};

// The words in a WAV file.
class Transcriber : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    // Empty when it can transcribe; otherwise what is missing, in English.
    virtual QString unavailableReason() const = 0;
    virtual void transcribe(int id, const QString &wavPath) = 0;
signals:
    void transcribed(int id, bool ok, const QString &text, const QString &error);
};

// ffmpeg reading PipeWire's default source through its PulseAudio interface.
class FfmpegAudioSource : public AudioSource {
    Q_OBJECT
public:
    explicit FfmpegAudioSource(QObject *parent = nullptr);
    ~FfmpegAudioSource() override;
    void start() override;
    void stop() override;

private:
    QProcess m_process;
    QByteArray m_carry;
    bool m_stopping = false;
};

// grim, which every Omarchy desktop has, capturing all outputs at once.
class GrimScreenshotter : public Screenshotter {
    Q_OBJECT
public:
    using Screenshotter::Screenshotter;
    void capture(int id, const QString &path) override;
};

// Superwhisper's offline models, through its command line. Files are
// transcribed one at a time, in the order they were said.
class SuperwhisperTranscriber : public Transcriber {
    Q_OBJECT
public:
    explicit SuperwhisperTranscriber(QObject *parent = nullptr);
    ~SuperwhisperTranscriber() override;
    QString unavailableReason() const override;
    void transcribe(int id, const QString &wavPath) override;

private:
    void next();
    void finish(bool ok, const QString &text, const QString &error);
    void read();

    struct Job {
        int id;
        QString path;
    };
    QString m_program;
    QQueue<Job> m_queue;
    bool m_busy = false;
    Job m_current{};
    qint64 m_session = -1;
    QProcess m_submit;
    QProcess m_reader;
    QTimer m_poll;
    QTimer m_timeout;
};
