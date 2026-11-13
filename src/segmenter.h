#pragma once

#include <QList>
#include <QtGlobal>
#include <optional>

// Splits a stream of 16-bit mono samples into utterances at the pauses
// between them, so each can be transcribed while the session goes on. A frame
// is speech when its level stands clearly above the room's noise floor, which
// it learns from the quiet frames as it goes.
class Segmenter {
public:
    struct Utterance {
        qint64 start = 0; // first sample
        qint64 end = 0;   // one past the last sample
    };

    explicit Segmenter(int sampleRate = 16000);

    // Returns the utterances that ended within these samples.
    QList<Utterance> feed(const qint16 *samples, qsizetype count);
    // Ends the utterance in progress, if any, at the last sample fed.
    std::optional<Utterance> flush();

    bool inSpeech() const { return m_inSpeech; }
    qint64 position() const { return m_position; }
    int sampleRate() const { return m_rate; }
    // The longest utterance, after which it is cut even without a pause, so a
    // long monologue still gets transcribed (and its screenshot taken) in time.
    qint64 maxLength() const { return m_maxLength; }

private:
    void frame(const qint16 *samples, int count, QList<Utterance> *out);

    int m_rate;
    int m_frameLength;
    qint64 m_padding;
    int m_framesToStart;
    int m_framesToEnd;
    qint64 m_maxLength;
    double m_minimumLevel = 350;

    double m_noise = 150;
    qint64 m_position = 0;
    qint64 m_lastEnd = 0;
    bool m_inSpeech = false;
    int m_speechRun = 0;
    int m_silenceRun = 0;
    qint64 m_runStart = 0;
    qint64 m_start = 0;
    qint64 m_lastSpeech = 0;
    QList<qint16> m_partial;
};
