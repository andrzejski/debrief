#pragma once

#include <QStringList>

// The commands that need no window: driving the running session, and setting
// up the bar widget and the agent skill. Returns -1 when the arguments ask for
// the window instead.
namespace Cli {
int run(const QStringList &arguments);
bool wantsWindow(const QStringList &arguments);
QString pluginSource(const QString &applicationDir);
} // namespace Cli
