#include "session.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QtEndian>
#include <algorithm>

namespace Debrief {

QString timecode(qint64 ms) {
    const qint64 seconds = std::max<qint64>(0, ms) / 1000;
    return QString("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QChar('0'))
        .arg(seconds / 60 % 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}

bool asksForScreenshot(const QString &text) {
    static const QRegularExpression notLetters("[^\\p{L}\\p{N}]+");
    static const QRegularExpression said("\\b(screen ?shot|screen ?cap|capture|snap)");
    return said.match(text.toLower().replace(notLetters, " ")).hasMatch();
}

QString slug(const QString &words, int maxWords) {
    static const QRegularExpression notLetters("[^\\p{L}\\p{N}]+");
    const QStringList all = words.toLower().split(notLetters, Qt::SkipEmptyParts);
    // A sentence is an answer that went wrong, not a name.
    if (all.isEmpty() || all.size() > maxWords * 2)
        return {};
    return all.mid(0, maxWords).join('-').left(48);
}

QString describe(const QStringList &sentences) {
    static const QRegularExpression notLetters("[^\\p{L}\\p{N}]+");
    static const QSet<QString> dull{
        "the", "and", "that", "this", "then", "than", "there", "here", "with", "have", "has", "had", "was", "were",
        "are", "but", "not", "does", "doesn", "did", "didn", "don", "isn", "can", "could", "would", "should",
        "will", "just", "like", "okay", "yeah", "yes", "well", "what", "when", "where", "why", "how", "which",
        "for", "from", "into", "onto", "out", "its", "it's", "you", "your", "they", "them", "been", "being",
        "about", "going", "gonna", "want", "now", "also", "some", "something", "thing", "see", "get", "got",
        "one", "let", "take", "screenshot", "screenshots", "screen", "shot", "screencap", "cap", "capture",
        "snap", "uhm", "hmm"};
    QHash<QString, int> count;
    QStringList order;
    for (const QString &sentence : sentences) {
        for (const QString &word : sentence.toLower().split(notLetters, Qt::SkipEmptyParts)) {
            if (word.size() < 3 || dull.contains(word))
                continue;
            if (count[word]++ == 0)
                order << word;
        }
    }
    QStringList most = order;
    std::stable_sort(most.begin(), most.end(), [&](const QString &a, const QString &b) { return count[a] > count[b]; });
    most = most.mid(0, 3);
    QStringList words;
    for (const QString &word : order)
        if (most.contains(word))
            words << word;
    return words.isEmpty() ? QStringLiteral("nothing-said") : slug(words.join(' '));
}

QString defaultRoot() {
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty())
        documents = QDir::homePath();
    return documents + "/Debrief";
}

QByteArray WavWriter::header(qint64 dataBytes, int sampleRate) {
    QByteArray h(44, '\0');
    char *p = h.data();
    auto u32 = [&](int at, quint32 v) { qToLittleEndian<quint32>(v, p + at); };
    auto u16 = [&](int at, quint16 v) { qToLittleEndian<quint16>(v, p + at); };
    memcpy(p, "RIFF", 4);
    u32(4, quint32(36 + dataBytes));
    memcpy(p + 8, "WAVEfmt ", 8);
    u32(16, 16);
    u16(20, 1); // PCM
    u16(22, 1); // mono
    u32(24, quint32(sampleRate));
    u32(28, quint32(sampleRate * 2));
    u16(32, 2);
    u16(34, 16);
    memcpy(p + 36, "data", 4);
    u32(40, quint32(dataBytes));
    return h;
}

bool WavWriter::write(const QString &path, const QByteArray &pcm, int sampleRate) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(header(pcm.size(), sampleRate)) == 44
        && file.write(pcm) == pcm.size();
}

bool WavWriter::open(const QString &path) {
    m_file.setFileName(path);
    m_bytes = 0;
    return m_file.open(QIODevice::WriteOnly) && m_file.write(header(0, m_rate)) == 44;
}

bool WavWriter::append(const QByteArray &pcm) {
    if (!m_file.isOpen())
        return false;
    const bool ok = m_file.write(pcm) == pcm.size();
    m_bytes += pcm.size();
    return ok;
}

void WavWriter::updateHeader() {
    if (!m_file.isOpen())
        return;
    const qint64 at = m_file.pos();
    m_file.seek(0);
    m_file.write(header(m_bytes, m_rate));
    m_file.seek(at);
    m_file.flush();
}

void WavWriter::close() {
    updateHeader();
    m_file.close();
}

} // namespace Debrief

using Debrief::timecode;

namespace {

QString speechText(const Speech &s) {
    switch (s.state) {
    case Speech::Pending: return "(still being transcribed)";
    case Speech::Failed: return "(not transcribed: " + s.error + ")";
    case Speech::Done: return s.text.isEmpty() ? "(no words recognized)" : s.text;
    }
    return {};
}

} // namespace

QString SessionLog::context(int index, int before, int after) const {
    QStringList words;
    for (int i = std::max(0, index - before); i <= std::min<int>(speech.size() - 1, index + after); ++i)
        if (speech[i].state == Speech::Done && !speech[i].text.isEmpty())
            words << speech[i].text;
    return words.join(' ');
}

QString SessionLog::markdown() const {
    QString md;
    md += "# Debrief session" + (title.isEmpty() ? QString() : ": " + title) + ", "
        + started.toString("yyyy-MM-dd HH:mm") + "\n\n";
    md += "- Audio: [" + audio.section('/', -1) + "](" + audio + "), beside this folder, " + timecode(durationMs)
        + (finished ? "" : " so far, still recording") + "\n";
    md += QString("- Screenshots: %1, in this folder\n").arg(shots.size());
    md += "- Each screenshot sits under the words it was taken during; session.json has the same links.\n\n";
    md += "## Transcript\n\n";

    // Speech in order, each followed by its screenshots; a screenshot with no
    // words of its own goes where its time puts it.
    struct Line { qint64 at; int order; QString text; };
    QList<Line> lines;
    for (int i = 0; i < speech.size(); ++i)
        lines.append({speech[i].startMs, 0,
                      "**[" + timecode(speech[i].startMs) + "]** " + speechText(speech[i]) + "\n\n"});
    for (const Shot &shot : shots) {
        const qint64 at = shot.speech >= 0 && shot.speech < speech.size() ? speech[shot.speech].startMs : shot.timeMs;
        lines.append({at, 1,
                      QString("![Screenshot %1 at %2](%3)\n\n").arg(shot.number).arg(timecode(shot.timeMs), shot.file)});
    }
    std::stable_sort(lines.begin(), lines.end(), [](const Line &a, const Line &b) {
        return a.at != b.at ? a.at < b.at : a.order < b.order;
    });
    for (const Line &line : lines)
        md += line.text;
    if (speech.isEmpty() && shots.isEmpty())
        md += "Nothing was said yet.\n";
    return md;
}

QJsonObject SessionLog::json() const {
    QJsonArray transcript;
    for (int i = 0; i < speech.size(); ++i) {
        const Speech &s = speech[i];
        QJsonArray linked;
        for (const Shot &shot : shots)
            if (shot.speech == i)
                linked.append(shot.number);
        QJsonObject entry{{"index", i},
                          {"start_ms", s.startMs},
                          {"end_ms", s.endMs},
                          {"start", timecode(s.startMs)},
                          {"end", timecode(s.endMs)},
                          {"status", s.state == Speech::Done ? "done" : s.state == Speech::Pending ? "pending" : "failed"},
                          {"text", s.text},
                          {"screenshots", linked}};
        if (s.state == Speech::Failed)
            entry.insert("error", s.error);
        transcript.append(entry);
    }
    QJsonArray screenshots;
    for (const Shot &shot : shots)
        screenshots.append(QJsonObject{{"number", shot.number},
                                       {"file", shot.file},
                                       {"time_ms", shot.timeMs},
                                       {"time", timecode(shot.timeMs)},
                                       {"source", shot.source},
                                       {"transcript_index", shot.speech},
                                       {"said", shot.speech >= 0 && shot.speech < speech.size() ? speech[shot.speech].text : QString()},
                                       {"context", shot.speech >= 0 ? context(shot.speech) : QString()}});
    return {{"version", 2},
            {"app", "debrief"},
            {"started", started.toString(Qt::ISODate)},
            {"duration_ms", durationMs},
            {"finished", finished},
            {"title", title},
            {"audio", audio},
            {"transcript_file", "transcript.md"},
            {"transcript", transcript},
            {"screenshots", screenshots}};
}

bool SessionLog::save(const QString &folder, QString *error) const {
    const QList<std::pair<QString, QByteArray>> files{
        {"transcript.md", markdown().toUtf8()},
        {"session.json", QJsonDocument(json()).toJson(QJsonDocument::Indented)}};
    for (const auto &[name, contents] : files) {
        QSaveFile file(folder + "/" + name);
        // A folder that can't take a temp file fails the save rather than
        // having the file truncated and rewritten in place.
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() || !file.commit()) {
            if (error)
                *error = file.errorString();
            return false;
        }
    }
    return true;
}
