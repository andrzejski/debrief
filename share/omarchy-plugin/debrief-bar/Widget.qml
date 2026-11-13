import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Ui

// Debrief in the bar: a bug while idle, a red record mark while a session is
// recording. Left click starts or stops, right click opens the window, middle
// click opens the session's folder. The app writes its state to a file in the
// runtime directory and then calls `omarchy-shell debrief.bar refresh`, so the
// widget reads it on each change rather than asking on a timer; the file
// watch alone misses a file that didn't exist yet when the shell started.
BarWidget {
  id: root
  moduleName: "debrief.bar"

  Translations { id: strings; directory: Qt.resolvedUrl("i18n") }

  readonly property string runtimeDir: Quickshell.env("XDG_RUNTIME_DIR") || "/tmp"
  property var status: ({})
  readonly property string sessionState: typeof status.state === "string" ? status.state : "idle"
  readonly property bool recording: sessionState === "recording"
  readonly property bool finishing: sessionState === "finishing"
  readonly property int shots: Math.max(0, Math.min(100000, Number(status.screenshots) || 0))
  readonly property string folder: typeof status.folder === "string" ? status.folder : ""

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  function load(text) {
    try {
      var parsed = JSON.parse(text)
      root.status = parsed && typeof parsed === "object" ? parsed : ({})
    } catch (e) {
      root.status = ({})
    }
  }

  function refresh() { stateFile.reload() }

  ShellIpc {
    target: "debrief.bar"

    function refresh(): void {
      root.broadcast("refresh")
    }
  }

  FileView {
    id: stateFile
    path: root.runtimeDir + "/debrief/state.json"
    watchChanges: true
    printErrors: false
    onFileChanged: reload()
    onLoaded: root.load(text())
    onLoadFailed: root.status = ({})
  }

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    // Nerd Font glyphs: bug, record, and an hourglass while finishing.
    text: String.fromCodePoint(root.recording ? 0xF044A : root.finishing ? 0xF051F : 0xF00E4)
    active: root.recording
    tooltipText: root.recording ? strings.tn("tooltip.recording", root.shots).arg(I18n.formatNumber(root.shots))
      : root.finishing ? strings.t("tooltip.finishing")
      : strings.t("tooltip.idle")
    onPressed: function(b) {
      if (!root.bar) return
      if (b === Qt.RightButton) root.bar.run("debrief open")
      else if (b === Qt.MiddleButton) {
        if (root.folder !== "") root.bar.run("xdg-open " + Util.shellQuote(root.folder))
      }
      else root.bar.run("debrief toggle")
    }
  }
}
