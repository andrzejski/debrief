import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 760
    height: 720
    minimumWidth: 520
    minimumHeight: 460
    visible: !startHidden
    title: "Debrief"
    color: theme.background

    Material.theme: theme.light ? Material.Light : Material.Dark
    Material.accent: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground

    readonly property bool rightToLeft: Qt.application.layoutDirection === Qt.RightToLeft
    readonly property color muted: theme.colors.muted
    readonly property color red: theme.colors.red

    function timecode(seconds) {
        function pad(n) { return n < 10 ? "0" + n : "" + n }
        return pad(Math.floor(seconds / 3600)) + ":" + pad(Math.floor(seconds / 60) % 60) + ":" + pad(seconds % 60)
    }

    // Black or white, whichever reads on a color (WCAG relative luminance).
    function readableOn(c) {
        function linear(v) { return v <= 0.04045 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4) }
        return 0.2126 * linear(c.r) + 0.7152 * linear(c.g) + 0.0722 * linear(c.b) > 0.179 ? "black" : "white"
    }

    function requestQuit() {
        if (backend.idle)
            Qt.quit()
        else
            quitDialog.open()
    }

    Shortcut { sequence: "?"; onActivated: help.opened ? help.close() : help.open() }
    Shortcut { sequence: "Q"; onActivated: win.requestQuit() }
    Shortcut { sequence: "Space"; onActivated: backend.toggle() }
    Shortcut { sequence: "S"; enabled: backend.recording; onActivated: backend.screenshotNow() }
    Shortcut { sequence: "O"; enabled: backend.folder !== ""; onActivated: backend.openFolder() }
    Shortcut { sequence: "B"; enabled: backend.folder !== "" && backend.idle; onActivated: backend.reportWithClaude() }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: win.rightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    objectName: "dot"
                    // A true circle keeps its fixed radius.
                    width: 14
                    height: 14
                    radius: 7
                    color: backend.recording ? win.red : backend.state === "finishing" ? theme.colors.yellow : win.muted
                    // Brighter while words are being heard.
                    opacity: backend.recording && !backend.speaking ? 0.55 : 1
                }

                Label {
                    objectName: "status"
                    text: backend.recording ? qsTr("Recording %1").arg(win.timecode(backend.elapsed))
                        : backend.state === "finishing" ? qsTr("Finishing the transcript…")
                        : qsTr("Ready")
                    font.pixelSize: 22
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Label {
                    objectName: "shotCount"
                    visible: backend.folder !== ""
                    text: qsTr("%Ln screenshots", "", backend.shotCount)
                    color: win.muted
                }
            }

            Label {
                objectName: "hint"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: win.muted
                text: backend.recording ? qsTr("Talk through what you're doing. Say “screenshot” and pause for a moment to capture the screen.")
                    : backend.state === "finishing" ? qsTr("Transcribing the last %Ln sentences.", "", backend.pendingCount)
                    : backend.folder !== "" ? qsTr("Saved in %1").arg(backend.folder)
                    : qsTr("Start a session, then debug out loud. Debrief records your voice, writes a transcript, and takes a screenshot whenever you say “screenshot”.")
            }

            Label {
                objectName: "notice"
                Layout.fillWidth: true
                visible: backend.notice !== ""
                wrapMode: Text.Wrap
                color: theme.colors.orange
                text: backend.notice
            }

            Rectangle {
                objectName: "errorBanner"
                Layout.fillWidth: true
                visible: backend.error !== ""
                implicitHeight: errorRow.implicitHeight + 16
                radius: theme.radius
                color: Qt.rgba(win.red.r, win.red.g, win.red.b, 0.15)
                border.color: win.red

                RowLayout {
                    id: errorRow
                    anchors.fill: parent
                    anchors.margins: 8
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: backend.error
                    }
                    ToolButton {
                        text: qsTr("Dismiss")
                        Material.roundedScale: theme.radius
                        focusPolicy: Qt.NoFocus
                        onClicked: backend.dismissError()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"
                radius: theme.radius
                border.color: theme.colors.selection

                ListView {
                    id: transcript
                    objectName: "transcript"
                    anchors.fill: parent
                    anchors.margins: 12
                    clip: true
                    spacing: 10
                    model: backend.entries
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    onCountChanged: Qt.callLater(transcript.positionViewAtEnd)

                    delegate: Item {
                        id: row
                        required property var modelData
                        width: ListView.view.width - 12
                        implicitHeight: modelData.kind === "shot" ? shotColumn.implicitHeight : speechRow.implicitHeight

                        RowLayout {
                            id: speechRow
                            visible: row.modelData.kind === "speech"
                            width: parent.width
                            spacing: 12

                            Label {
                                Layout.alignment: Qt.AlignTop
                                text: row.modelData.time
                                color: win.muted
                                font.family: "monospace"
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                font.italic: row.modelData.state !== "done"
                                color: row.modelData.state === "done" && row.modelData.text !== "" ? theme.foreground : win.muted
                                text: row.modelData.kind !== "speech" ? ""
                                    : row.modelData.state === "pending" ? qsTr("Transcribing…")
                                    : row.modelData.state === "failed" ? qsTr("Not transcribed: %1").arg(row.modelData.error)
                                    : row.modelData.text !== "" ? row.modelData.text
                                    : qsTr("No words recognized")
                            }
                        }

                        ColumnLayout {
                            id: shotColumn
                            visible: row.modelData.kind === "shot"
                            width: parent.width
                            spacing: 6

                            Label {
                                text: row.modelData.kind !== "shot" ? ""
                                    : row.modelData.source === "manual" ? qsTr("Screenshot %1 at %2, taken by hand").arg(row.modelData.number).arg(row.modelData.time)
                                    : qsTr("Screenshot %1 at %2").arg(row.modelData.number).arg(row.modelData.time)
                                color: theme.accent
                                font.bold: true
                            }
                            Image {
                                Layout.fillWidth: true
                                Layout.preferredHeight: row.modelData.kind === "shot" ? Math.min(240, width * 9 / 16) : 0
                                source: row.modelData.kind === "shot" ? row.modelData.image : ""
                                sourceSize.width: 640
                                fillMode: Image.PreserveAspectFit
                                horizontalAlignment: win.rightToLeft ? Image.AlignRight : Image.AlignLeft
                                asynchronous: true
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Qt.openUrlExternally(row.modelData.image)
                                }
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        width: parent.width * 0.8
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        visible: transcript.count === 0
                        color: win.muted
                        text: backend.recording ? qsTr("Listening…") : qsTr("The transcript and screenshots appear here.")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    id: primary
                    objectName: "primary"
                    text: backend.recording ? qsTr("Stop session") : qsTr("Start session")
                    enabled: backend.state !== "finishing"
                    Material.roundedScale: theme.radius
                    focusPolicy: Qt.NoFocus
                    onClicked: backend.toggle()
                    // A highlighted button colors its caption by Material's own
                    // rule, not Material.foreground, so it can't read on every accent.
                    contentItem: Label {
                        objectName: "primaryCaption"
                        text: primary.text
                        font: primary.font
                        color: backend.recording ? win.readableOn(win.red) : theme.accentForeground
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    // Drawn here rather than by Material, so it is the accent
                    // (red while recording) on every theme.
                    background: Rectangle {
                        implicitWidth: 64
                        implicitHeight: 40
                        y: primary.topInset
                        height: primary.height - primary.topInset - primary.bottomInset
                        radius: Math.min(theme.radius, height / 2)
                        color: backend.recording ? win.red : theme.accent
                        opacity: !primary.enabled ? 0.4 : primary.down ? 0.8 : primary.hovered ? 0.92 : 1
                    }
                }

                Button {
                    text: qsTr("Screenshot now")
                    enabled: backend.recording
                    flat: true
                    Material.roundedScale: theme.radius
                    focusPolicy: Qt.NoFocus
                    onClicked: backend.screenshotNow()
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Open folder")
                    enabled: backend.folder !== ""
                    flat: true
                    Material.roundedScale: theme.radius
                    focusPolicy: Qt.NoFocus
                    onClicked: backend.openFolder()
                }

                Button {
                    objectName: "report"
                    text: qsTr("Bug report with Claude")
                    enabled: backend.folder !== "" && backend.idle
                    Material.roundedScale: theme.radius
                    focusPolicy: Qt.NoFocus
                    onClicked: backend.reportWithClaude()
                }
            }
        }
    }

    Popup {
        id: help
        objectName: "help"
        anchors.centerIn: Overlay.overlay
        modal: true
        focus: true
        padding: 24
        background: Rectangle {
            color: theme.background
            border.color: theme.colors.selection
            radius: theme.radius
        }

        ColumnLayout {
            spacing: 8
            // A popup is not an Item and doesn't inherit the window's
            // mirroring, so its content opts in.
            LayoutMirroring.enabled: win.rightToLeft
            LayoutMirroring.childrenInherit: true

            Repeater {
                model: [
                    { keys: "Space", action: qsTr("Start or stop a session") },
                    { keys: "S", action: qsTr("Take a screenshot now") },
                    { keys: "O", action: qsTr("Open the session folder") },
                    { keys: "B", action: qsTr("Write a bug report with Claude") },
                    { keys: "?", action: qsTr("Show or hide the shortcuts") },
                    { keys: "Q", action: qsTr("Quit") }
                ]
                delegate: RowLayout {
                    required property var modelData
                    spacing: 24

                    Label {
                        Layout.preferredWidth: 64
                        text: modelData.keys
                        color: theme.accent
                        font.bold: true
                    }
                    Label { text: modelData.action }
                }
            }
        }
    }

    Dialog {
        id: quitDialog
        objectName: "quitDialog"
        anchors.centerIn: Overlay.overlay
        modal: true
        focus: true
        title: qsTr("Stop the session and quit?")
        background: Rectangle {
            color: theme.background
            border.color: theme.colors.selection
            radius: theme.radius
        }

        Label {
            LayoutMirroring.enabled: win.rightToLeft
            width: 360
            wrapMode: Text.Wrap
            text: qsTr("What has been recorded is kept, and Debrief quits once the transcript is finished.")
        }

        footer: DialogButtonBox {
            LayoutMirroring.enabled: win.rightToLeft
            LayoutMirroring.childrenInherit: true
            Button {
                text: qsTr("Keep recording")
                flat: true
                Material.roundedScale: theme.radius
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                objectName: "stopAndQuit"
                text: qsTr("Stop and quit")
                flat: true
                Material.roundedScale: theme.radius
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }

        // Escape or closing it keeps recording: the answer that loses nothing.
        onAccepted: {
            backend.stop()
            // The app quits when there's no window and the session is saved.
            win.close()
        }
    }
}
