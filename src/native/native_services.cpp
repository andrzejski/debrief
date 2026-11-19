#include "native_services.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMediaDevices>
#include <QPainter>
#ifdef Q_OS_MACOS
#include <QPermissions>
#endif
#include <QPixmap>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QThreadPool>
#include <algorithm>
#include <cstring>
#include <thread>
#include <vector>
#include <whisper.h>

// ---- AudioConverter ---------------------------------------------------------

AudioConverter::AudioConverter(const QAudioFormat &from, int toRate) : m_from(from), m_toRate(toRate) {}

QByteArray AudioConverter::convert(const QByteArray &bytes) {
    const int channels = std::max(1, m_from.channelCount());
    const int width = m_from.bytesPerSample();
    const int frame = channels * width;
    const qint64 fromRate = m_from.sampleRate();
    if (width <= 0 || fromRate <= 0)
        return {};

    m_carry += bytes;
    const qsizetype frames = m_carry.size() / frame;
    QByteArray out;
    out.reserve(qsizetype(frames * m_toRate / fromRate + 2) * 2);
    const char *data = m_carry.constData();
    for (qsizetype i = 0; i < frames; ++i) {
        // One sample for the frame: the average of its channels, as -1..1.
        float mono = 0;
        for (int c = 0; c < channels; ++c) {
            const char *at = data + i * frame + c * width;
            switch (m_from.sampleFormat()) {
            case QAudioFormat::UInt8:
                mono += (quint8(*at) - 128) / 128.f;
                break;
            case QAudioFormat::Int16: {
                qint16 v;
                std::memcpy(&v, at, sizeof v);
                mono += v / 32768.f;
                break;
            }
            case QAudioFormat::Int32: {
                qint32 v;
                std::memcpy(&v, at, sizeof v);
                mono += v / 2147483648.f;
                break;
            }
            case QAudioFormat::Float: {
                float v;
                std::memcpy(&v, at, sizeof v);
                mono += v;
                break;
            }
            default:
                break;
            }
        }
        mono /= channels;

        // Down to the session's rate by averaging the input samples each
        // output sample covers, which also keeps what is above its range
        // from folding into speech. A slower microphone repeats samples.
        m_sum += mono;
        ++m_count;
        m_phase += m_toRate;
        while (m_phase >= fromRate) {
            m_phase -= fromRate;
            if (m_count > 0)
                m_last = float(m_sum / m_count);
            m_sum = 0;
            m_count = 0;
            const qint16 sample = qint16(std::clamp(m_last, -1.f, 1.f) * 32767.f);
            out.append(reinterpret_cast<const char *>(&sample), sizeof sample);
        }
    }
    m_carry.remove(0, frames * frame);
    return out;
}

// ---- QtAudioSource ----------------------------------------------------------

QtAudioSource::~QtAudioSource() { release(); }

void QtAudioSource::release() {
    if (!m_source)
        return;
    m_source->disconnect(this);
    if (m_device)
        m_device->disconnect(this);
    m_source->stop();
    m_source->deleteLater();
    m_source = nullptr;
    m_device = nullptr;
}

void QtAudioSource::start() {
    release();
    m_stopping = false;
#ifdef Q_OS_MACOS
    // macOS asks the user once; recording starts when they have said yes.
    const QMicrophonePermission microphoneAccess;
    switch (qApp->checkPermission(microphoneAccess)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(microphoneAccess, this, [this](const QPermission &answer) {
            if (m_stopping)
                return;
            if (answer.status() == Qt::PermissionStatus::Granted)
                start();
            else
                emit failed(tr("Debrief isn't allowed to use the microphone. Turn it on in System Settings, under "
                               "Privacy & Security, Microphone."));
        });
        return;
    case Qt::PermissionStatus::Denied:
        emit failed(tr("Debrief isn't allowed to use the microphone. Turn it on in System Settings, under "
                       "Privacy & Security, Microphone."));
        return;
    case Qt::PermissionStatus::Granted:
        break;
    }
#endif
    const QAudioDevice microphone = QMediaDevices::defaultAudioInput();
    if (microphone.isNull()) {
        emit failed(tr("No microphone was found, so nothing can be recorded."));
        return;
    }
    // Asked for as a session wants it; a microphone that can't is recorded
    // as it comes and converted.
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!microphone.isFormatSupported(format))
        format = microphone.preferredFormat();
    m_converter = AudioConverter(format, 16000);

    m_source = new QAudioSource(microphone, format, this);
    connect(m_source, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
        if (state != QAudio::StoppedState || m_stopping || !m_source || m_source->error() == QAudio::NoError)
            return;
        release();
        emit failed(tr("The microphone stopped recording. Check that it is still connected and that apps are "
                       "allowed to use it."));
    });
    m_device = m_source->start();
    if (!m_device || m_source->error() != QAudio::NoError) {
        release();
        emit failed(tr("Couldn't open the microphone. Check that apps are allowed to use it."));
        return;
    }
    connect(m_device, &QIODevice::readyRead, this, [this] {
        const QByteArray pcm = m_converter.convert(m_device->readAll());
        if (!pcm.isEmpty())
            emit audio(pcm);
    });
}

void QtAudioSource::stop() {
    m_stopping = true;
    if (m_device) {
        const QByteArray pcm = m_converter.convert(m_device->readAll());
        if (!pcm.isEmpty())
            emit audio(pcm);
    }
    release();
    emit stopped();
}

// ---- QtScreenshotter --------------------------------------------------------

void QtScreenshotter::capture(int id, const QString &path) {
    struct Piece {
        QImage image;
        QPoint at;
    };
    QList<Piece> pieces;
    QRect bounds;
    for (QScreen *screen : QGuiApplication::screens()) {
        QImage image = screen->grabWindow(0).toImage();
        if (image.isNull())
            continue;
        // In the screen's own pixels, placed where the screen starts.
        image.setDevicePixelRatio(1);
        const QPoint at = screen->geometry().topLeft();
        bounds |= QRect(at, image.size());
        pieces.append({image, at});
    }
    const QPointer<QtScreenshotter> self(this);
    auto answer = [self, id](bool ok, const QString &error) {
        // Back on the GUI thread, and only if the session is still there.
        QMetaObject::invokeMethod(
            qApp, [self, id, ok, error] { if (self) emit self->captured(id, ok, error); }, Qt::QueuedConnection);
    };
    if (pieces.isEmpty()) {
        answer(false, tr("The screen couldn't be read."));
        return;
    }
    // Encoding a large PNG takes long enough to be felt, so not here.
    QThreadPool::globalInstance()->start([pieces, bounds, path, answer] {
        QImage whole = pieces.first().image;
        if (pieces.size() > 1) {
            whole = QImage(bounds.size(), QImage::Format_RGB32);
            whole.fill(Qt::black);
            QPainter painter(&whole);
            for (const Piece &piece : pieces)
                painter.drawImage(piece.at - bounds.topLeft(), piece.image);
        }
        // Light compression: a capture lands within a moment of the words.
        const bool ok = whole.save(path, "PNG", 80);
        answer(ok, ok ? QString() : QtScreenshotter::tr("Couldn't write %1.").arg(QDir::toNativeSeparators(path)));
    });
}

// ---- WhisperWorker ----------------------------------------------------------

WhisperWorker::WhisperWorker(const QString &model) : m_model(model), m_idle(this) {
    // Nothing whisper says is for the user; errors come back as results.
    whisper_log_set([](ggml_log_level, const char *, void *) {}, nullptr);
    m_idle.setSingleShot(true);
    m_idle.setInterval(120000);
    connect(&m_idle, &QTimer::timeout, this, &WhisperWorker::unload);
}

WhisperWorker::~WhisperWorker() { unload(); }

void WhisperWorker::unload() {
    if (m_context)
        whisper_free(m_context);
    m_context = nullptr;
}

void WhisperWorker::transcribe(int id, const QString &wavPath) {
    if (m_abort)
        return;
    m_idle.stop();
    if (!m_context) {
        // Read through Qt rather than by name, so a folder with letters
        // outside the system's code page still opens on Windows.
        QFile file(m_model);
        uchar *bytes = file.open(QIODevice::ReadOnly) ? file.map(0, file.size()) : nullptr;
        if (bytes) {
            whisper_context_params params = whisper_context_default_params();
            params.use_gpu = false;
            m_context = whisper_init_from_buffer_with_params(bytes, size_t(file.size()), params);
            file.unmap(bytes);
        }
        if (!m_context) {
            emit transcribed(id, false, {},
                             tr("Couldn't load the speech model %1.").arg(QDir::toNativeSeparators(m_model)));
            return;
        }
    }

    QFile wav(wavPath);
    if (!wav.open(QIODevice::ReadOnly)) {
        emit transcribed(id, false, {}, tr("Couldn't read the recording."));
        return;
    }
    // Debrief's own files: a 44-byte header, then 16 kHz mono 16-bit samples.
    const QByteArray pcm = wav.readAll().mid(44);
    const qsizetype count = pcm.size() / 2;
    // Whisper skips anything much shorter than a second, so a single word is
    // given silence to follow it.
    std::vector<float> samples(std::max<qsizetype>(count, 16000 * 5 / 4), 0.f);
    const auto *in = reinterpret_cast<const qint16 *>(pcm.constData());
    for (qsizetype i = 0; i < count; ++i)
        samples[i] = in[i] / 32768.f;

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    // Half the machine at most: the user is busy debugging on the rest.
    params.n_threads = std::clamp(int(std::thread::hardware_concurrency()) / 2, 1, 4);
    params.language = whisper_is_multilingual(m_context) ? "auto" : "en";
    params.translate = false;
    params.no_context = true;
    params.no_timestamps = true;
    params.suppress_blank = true;
    params.suppress_nst = true;
    params.print_progress = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.print_special = false;
    params.abort_callback = [](void *abort) { return static_cast<std::atomic<bool> *>(abort)->load(); };
    params.abort_callback_user_data = &m_abort;

    if (whisper_full(m_context, params, samples.data(), int(samples.size())) != 0) {
        if (!m_abort)
            emit transcribed(id, false, {}, tr("The speech model couldn't transcribe this."));
        return;
    }
    QString text;
    for (int i = 0, n = whisper_full_n_segments(m_context); i < n; ++i)
        text += QString::fromUtf8(whisper_full_get_segment_text(m_context, i));
    emit transcribed(id, true, WhisperTranscriber::clean(text), {});
    m_idle.start();
}

// ---- WhisperTranscriber -----------------------------------------------------

WhisperTranscriber::WhisperTranscriber(const QString &model, QObject *parent)
    : Transcriber(parent), m_model(model.isEmpty() ? findModel() : model) {
    if (m_model.isEmpty())
        return;
    m_worker = new WhisperWorker(m_model);
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &WhisperTranscriber::requested, m_worker, &WhisperWorker::transcribe);
    connect(m_worker, &WhisperWorker::transcribed, this, &Transcriber::transcribed);
    m_thread.start();
}

WhisperTranscriber::~WhisperTranscriber() {
    if (!m_worker)
        return;
    m_worker->abort();
    m_thread.quit();
    m_thread.wait();
}

QString WhisperTranscriber::findModel() {
    const QString chosen = qEnvironmentVariable("DEBRIEF_MODEL");
    if (!chosen.isEmpty())
        return QFile::exists(chosen) ? chosen : QString();
    const QStringList folders{
        QCoreApplication::applicationDirPath() + "/models",
        // Inside Debrief.app on macOS.
        QCoreApplication::applicationDirPath() + "/../Resources/models",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models",
    };
    for (const QString &folder : folders) {
        const QStringList models = QDir(folder).entryList({"ggml-*.bin"}, QDir::Files, QDir::Name);
        if (!models.isEmpty())
            return folder + "/" + models.first();
    }
    return {};
}

QString WhisperTranscriber::clean(const QString &text) {
    static const QRegularExpression notes(R"(\[[^\]]*\]|\([^)]*\)|\*[^*]*\*)");
    return QString(text).remove(notes).simplified();
}

QString WhisperTranscriber::unavailableReason() const {
    return m_model.isEmpty() ? tr("No speech model was found in the models folder beside Debrief, so there is no "
                                  "transcript and saying “screenshot” does nothing. Audio still records, and S "
                                  "takes a screenshot.")
                             : QString();
}

void WhisperTranscriber::transcribe(int id, const QString &wavPath) {
    if (!m_worker) {
        // Asynchronously, like an answer.
        QTimer::singleShot(0, this, [this, id] { emit transcribed(id, false, {}, tr("No speech model was found.")); });
        return;
    }
    emit requested(id, wavPath);
}
