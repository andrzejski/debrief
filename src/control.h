#pragma once

#include <QJsonObject>
#include <QLocalServer>
#include <QObject>
#include <optional>

class Backend;

// The running app answers `debrief start|stop|toggle|shot|show|status` on a
// local socket, so the bar, a key binding, or an agent drives the one session
// there is, and launching Debrief again raises its window instead of a second.
class Control : public QObject {
    Q_OBJECT
public:
    Control(Backend *backend, const QString &socketPath = {}, QObject *parent = nullptr);
    bool listen();
    QString errorString() const { return m_server.errorString(); }

    static QString defaultSocket();
    // Sends one command to the running app; nothing when none is running.
    static std::optional<QJsonObject> send(const QString &command, const QString &socketPath = {});

signals:
    void showRequested();

private:
    QJsonObject handle(const QString &command);

    Backend *m_backend;
    QString m_path;
    QLocalServer m_server;
};
