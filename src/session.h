#pragma once

#include <QDateTime>
#include <QFile>
#include <QJsonObject>
#include <QList>
#include <QString>

// What a debugging session is on disk: the transcript, the screenshots, and
// which words each screenshot belongs to. It writes transcript.md for people
// and session.json for agents, next to the screenshots, so the one folder can
// be handed to Claude; the recording sits beside the folder under its name.
struct Speech {
    enum State { Pending, Done, Failed };
    qint64 startMs = 0;
    qint64 endMs = 0;
    QString text;
    State state = Pending;
    QString error;
};

struct Shot {
    int number = 0;
    qint64 timeMs = 0;
    QString file;            // relative to the session folder
    QString source;          // "voice" (it was asked for out loud) or "manual"
    int speech = -1;         // index of the utterance it belongs to, or -1
};

struct SessionLog {
    QDateTime started;
    qint64 durationMs = 0;
    bool finished = false;
    QString title;                    // two or three words for what it was about, once it is finished
    QString audio = "../audio.wav";   // the recording, relative to the session folder it sits beside
    QList<Speech> speech;
    QList<Shot> shots;

    QString markdown() const;
    QJsonObject json() const;
    // Writes transcript.md and session.json atomically.
    bool save(const QString &folder, QString *error) const;
    // The words around an utterance, for a screenshot's context.
    QString context(int speechIndex, int before = 2, int after = 2) const;
};

namespace Debrief {

// HH:MM:SS from milliseconds.
QString timecode(qint64 ms);
// Whether a screenshot was asked for: "screenshot", "screencap", "capture",
// or "snap", and what they grow into ("screen shot", "screenshots",
// "captured", "snapshot"), in whatever case and punctuation the transcription
// came back with.
bool asksForScreenshot(const QString &text);
// The first few words of a description as the end of a folder name:
// "Approve flow stalls." is "approve-flow-stalls". Empty when there are no
// words, or so many that it isn't a name.
QString slug(const QString &words, int maxWords = 3);
// A name from the transcript itself, for when nothing better could be asked
// for: the words said most, in the order they were first said.
QString describe(const QStringList &sentences);
// The root that sessions are saved under: ~/Documents/Debrief.
QString defaultRoot();

// A 16-bit mono PCM WAV file, written as the audio arrives, with its sizes
// brought up to date along the way so a crash still leaves a playable file.
class WavWriter {
public:
    explicit WavWriter(int sampleRate = 16000) : m_rate(sampleRate) {}
    bool open(const QString &path);
    bool append(const QByteArray &pcm);
    void updateHeader();
    void close();
    bool isOpen() const { return m_file.isOpen(); }
    QString errorString() const { return m_file.errorString(); }

    static QByteArray header(qint64 dataBytes, int sampleRate);
    static bool write(const QString &path, const QByteArray &pcm, int sampleRate);

private:
    int m_rate;
    QFile m_file;
    qint64 m_bytes = 0;
};

} // namespace Debrief
