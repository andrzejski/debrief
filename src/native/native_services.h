#pragma once

#include <QAudioFormat>
#include <QPointer>
#include <QThread>
#include <atomic>

#include "services.h"

class QAudioSource;
class QIODevice;
struct whisper_context;

// The same services with nothing to install beside the app: Qt records the
// microphone and reads the screen, and whisper.cpp, linked in, transcribes
// with a model file that ships in the app's folder. This is what Windows
// uses, where there is no ffmpeg, grim, or Superwhisper command line.

// Turns whatever a microphone delivers (any rate, channel count, and sample
// format) into the 16 kHz mono 16-bit audio a session works in.
class AudioConverter {
public:
    AudioConverter() = default;
    AudioConverter(const QAudioFormat &from, int toRate);
    QByteArray convert(const QByteArray &bytes);

private:
    QAudioFormat m_from;
    int m_toRate = 16000;
    QByteArray m_carry;  // part of a frame, waiting for the rest
    double m_sum = 0;    // input samples since the last output sample
    int m_count = 0;
    qint64 m_phase = 0;
    float m_last = 0;
};

// The default microphone, through Qt Multimedia.
class QtAudioSource : public AudioSource {
    Q_OBJECT
public:
    using AudioSource::AudioSource;
    ~QtAudioSource() override;
    void start() override;
    void stop() override;

private:
    void release();

    QAudioSource *m_source = nullptr;
    QPointer<QIODevice> m_device;
    AudioConverter m_converter;
    bool m_stopping = false;
};

// Every screen, side by side as they are arranged, read on the GUI thread and
// written to a PNG off it.
class QtScreenshotter : public Screenshotter {
    Q_OBJECT
public:
    using Screenshotter::Screenshotter;
    void capture(int id, const QString &path) override;
};

// Runs on its own thread, one file at a time in the order they were said.
// The model is loaded for the first file and let go after a while without
// one, so an idle Debrief holds on to none of it.
class WhisperWorker : public QObject {
    Q_OBJECT
public:
    explicit WhisperWorker(const QString &model);
    ~WhisperWorker() override;
    void abort() { m_abort = true; }

public slots:
    void transcribe(int id, const QString &wavPath);

signals:
    void transcribed(int id, bool ok, const QString &text, const QString &error);

private:
    void unload();

    QString m_model;
    whisper_context *m_context = nullptr;
    QTimer m_idle;
    std::atomic<bool> m_abort{false};
};

class WhisperTranscriber : public Transcriber {
    Q_OBJECT
public:
    // Tests pass their own model file.
    explicit WhisperTranscriber(const QString &model = {}, QObject *parent = nullptr);
    ~WhisperTranscriber() override;
    QString unavailableReason() const override;
    void transcribe(int id, const QString &wavPath) override;

    // The model that ships with the app: DEBRIEF_MODEL, or the first
    // ggml-*.bin in models/ beside the program or in the app's data folder.
    static QString findModel();
    // What whisper heard, without the notes it adds for sounds, like
    // [BLANK_AUDIO] or (keyboard clicking).
    static QString clean(const QString &text);

signals:
    void requested(int id, const QString &wavPath);

private:
    QString m_model;
    QThread m_thread;
    WhisperWorker *m_worker = nullptr;
};
