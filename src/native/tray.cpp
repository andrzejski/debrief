#include "tray.h"

#include <QDir>
#include <QPainter>

#include "backend.h"
#include "session.h"

Tray::Tray(Backend *backend, QObject *parent) : QObject(parent), m_backend(backend) {
    m_toggle = m_menu.addAction(QString(), backend, &Backend::toggle);
    m_shot = m_menu.addAction(tr("Screenshot now"), backend, &Backend::screenshotNow);
    m_menu.addSeparator();
    m_menu.addAction(tr("Open Debrief"), this, &Tray::showRequested);
    m_folder = m_menu.addAction(tr("Open session folder"), backend, &Backend::openFolder);
    m_menu.addAction(tr("Open all sessions"), backend, &Backend::openRoot);
    m_menu.addSeparator();
    m_menu.addAction(tr("Quit"), this, &Tray::quitRequested);
    m_icon.setContextMenu(&m_menu);

    connect(&m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            emit showRequested();
    });
    connect(&m_icon, &QSystemTrayIcon::messageClicked, this, [this] {
        if (m_saved)
            m_backend->openFolder();
    });
    connect(backend, &Backend::stateChanged, this, &Tray::refresh);
    connect(backend, &Backend::folderChanged, this, &Tray::refresh);
    connect(backend, &Backend::finished, this, [this](const QString &folder) {
        const SessionLog &log = m_backend->log();
        m_saved = true;
        m_icon.showMessage(tr("Debrief session saved"),
                           tr("%Ln screenshots, %1 of audio, in %2", "", log.shots.size())
                               .arg(Debrief::timecode(log.durationMs), QDir::toNativeSeparators(folder)),
                           icon(false));
    });
    refresh();
    m_icon.show();
}

bool Tray::available() { return QSystemTrayIcon::isSystemTrayAvailable(); }

void Tray::sayStillRunning() {
    if (m_said)
        return;
    m_said = true;
    m_saved = false;
    m_icon.showMessage(tr("Debrief is still running"),
                       tr("Find it in the notification area, by the clock. Right-click its icon to start a "
                          "session or to quit."),
                       icon(false));
}

void Tray::refresh() {
    const bool recording = m_backend->recording();
    m_toggle->setText(recording ? tr("Stop session") : tr("Start session"));
    m_toggle->setEnabled(m_backend->state() != "finishing");
    m_shot->setEnabled(recording);
    m_folder->setEnabled(!m_backend->folder().isEmpty());
    m_icon.setIcon(icon(recording));
    m_icon.setToolTip(recording ? tr("Debrief: recording")
                      : m_backend->state() == "finishing" ? tr("Debrief: finishing the transcript")
                      : tr("Debrief"));
}

QIcon Tray::icon(bool recording) const {
    const QIcon app(":/debrief.svg");
    if (!recording)
        return app;
    QIcon marked;
    for (int size : {16, 20, 24, 32, 48, 64}) {
        QPixmap pixmap = app.pixmap(size, size);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(Qt::white, size / 16.0));
        painter.setBrush(QColor("#e5322d"));
        const qreal d = size * 0.56;
        painter.drawEllipse(QRectF(size - d - 0.5, size - d - 0.5, d, d));
        painter.end();
        marked.addPixmap(pixmap);
    }
    return marked;
}
