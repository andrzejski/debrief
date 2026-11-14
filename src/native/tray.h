#pragma once

#include <QIcon>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

class Backend;

// Debrief in the notification area: where it lives on Windows while its
// window is closed, the way the bar widget does on Omarchy. The icon gains a
// red dot while a session records, and its menu drives the session.
class Tray : public QObject {
    Q_OBJECT
public:
    explicit Tray(Backend *backend, QObject *parent = nullptr);

    static bool available();
    // Said once, the first time the window closes and Debrief stays.
    void sayStillRunning();

signals:
    void showRequested();
    void quitRequested();

private:
    void refresh();
    QIcon icon(bool recording) const;

    Backend *m_backend;
    QSystemTrayIcon m_icon;
    QMenu m_menu;
    QAction *m_toggle = nullptr;
    QAction *m_shot = nullptr;
    QAction *m_folder = nullptr;
    bool m_said = false;
    bool m_saved = false;  // the last notification was a saved session, which a click opens
};
