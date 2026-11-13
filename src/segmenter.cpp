#include "segmenter.h"

#include <cmath>

Segmenter::Segmenter(int sampleRate)
    : m_rate(sampleRate),
      m_frameLength(sampleRate * 30 / 1000),
      // A little audio either side keeps the first and last syllables.
      m_padding(sampleRate * 300 / 1000),
      // 90 ms of sound starts an utterance, so a click or a cough doesn't.
      m_framesToStart(3),
      // 800 ms of quiet ends one: long enough for a breath mid-sentence,
      // short enough that "screenshot" is acted on soon after it is said.
      m_framesToEnd(800 / 30),
      m_maxLength(qint64(sampleRate) * 15) {}

QList<Segmenter::Utterance> Segmenter::feed(const qint16 *samples, qsizetype count) {
    QList<Utterance> out;
    qsizetype i = 0;
    if (!m_partial.isEmpty()) {
        while (i < count && m_partial.size() < m_frameLength)
            m_partial.append(samples[i++]);
        if (m_partial.size() < m_frameLength)
            return out;
        frame(m_partial.constData(), m_frameLength, &out);
        m_partial.clear();
    }
    for (; i + m_frameLength <= count; i += m_frameLength)
        frame(samples + i, m_frameLength, &out);
    for (; i < count; ++i)
        m_partial.append(samples[i]);
    return out;
}

void Segmenter::frame(const qint16 *samples, int count, QList<Utterance> *out) {
    double sum = 0;
    for (int i = 0; i < count; ++i)
        sum += double(samples[i]) * samples[i];
    const double level = std::sqrt(sum / count);
    const qint64 frameStart = m_position;
    m_position += count;

    const bool loud = level > std::max(m_minimumLevel, m_noise * 3);
    if (!loud && !m_inSpeech)
        // The noise floor follows the room slowly, and only while it's quiet.
        m_noise = m_noise * 0.97 + level * 0.03;

    if (!m_inSpeech) {
        if (loud) {
            if (m_speechRun++ == 0)
                m_runStart = frameStart;
            if (m_speechRun >= m_framesToStart) {
                m_inSpeech = true;
                m_silenceRun = 0;
                m_start = std::max(m_lastEnd, m_runStart - m_padding);
                m_lastSpeech = m_position;
            }
        } else {
            m_speechRun = 0;
        }
        return;
    }

    if (loud) {
        m_silenceRun = 0;
        m_lastSpeech = m_position;
    } else if (++m_silenceRun >= m_framesToEnd) {
        const qint64 end = std::min(m_position, m_lastSpeech + m_padding);
        out->append({m_start, end});
        m_lastEnd = end;
        m_inSpeech = false;
        m_speechRun = 0;
        return;
    }

    if (m_position - m_start >= m_maxLength) {
        out->append({m_start, m_position});
        m_lastEnd = m_start = m_position;
    }
}

std::optional<Segmenter::Utterance> Segmenter::flush() {
    if (!m_inSpeech)
        return std::nullopt;
    m_inSpeech = false;
    m_speechRun = 0;
    const Utterance utterance{m_start, m_position};
    m_lastEnd = m_position;
    return utterance;
}
