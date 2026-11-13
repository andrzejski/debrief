#include "control.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>

#include "backend.h"

Control::Control(Backend *backend, const QString &socketPath, QObject *parent)
    : QObject(parent), m_backend(backend), m_path(socketPath.isEmpty() ? defaultSocket() : socketPath) {
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, socket, [this, socket] {
                if (!socket->canReadLine())
                    return;
                const QString command = QString::fromUtf8(socket->readLine()).trimmed();
                socket->write(QJsonDocument(handle(command)).toJson(QJsonDocument::Compact) + "\n");
                socket->flush();
                socket->disconnectFromServer();
            });
        }
    });
}

QString Control::defaultSocket() {
    QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty())
        runtime = QDir::tempPath();
    return runtime + "/debrief/control.sock";
}

bool Control::listen() {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    // Left behind by an app that didn't get to clean up; a live one was
    // already asked before this.
    QLocalServer::removeServer(m_path);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    return m_server.listen(m_path);
}

QJsonObject Control::handle(const QString &command) {
    if (command == "start")
        m_backend->start();
    else if (command == "stop")
        m_backend->stop();
    else if (command == "toggle")
        m_backend->toggle();
    else if (command == "shot") {
        if (!m_backend->recording())
            return {{"ok", false}, {"error", "No session is recording."}};
        m_backend->screenshotNow();
    } else if (command == "show")
        emit showRequested();
    else if (command != "status")
        return {{"ok", false}, {"error", "Unknown command: " + command}};
    QJsonObject reply = m_backend->status();
    reply.insert("ok", true);
    return reply;
}

std::optional<QJsonObject> Control::send(const QString &command, const QString &socketPath) {
    QLocalSocket socket;
    socket.connectToServer(socketPath.isEmpty() ? defaultSocket() : socketPath);
    if (!socket.waitForConnected(1000))
        return std::nullopt;
    socket.write(command.toUtf8() + "\n");
    socket.flush();
    while (!socket.canReadLine())
        if (!socket.waitForReadyRead(3000))
            return std::nullopt;
    return QJsonDocument::fromJson(socket.readLine()).object();
}
