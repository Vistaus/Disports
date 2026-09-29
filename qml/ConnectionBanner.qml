import QtQuick
import Lomiri.Components
import Disports.Core

// Shown while a signed-in session is not connected: offline, or waiting to
// reconnect. Tapping it retries right away.
Rectangle {
    id: banner

    readonly property bool shown: Session.phase === SessionPhase.Ready && !Session.connected

    height: shown ? units.gu(4) : 0
    clip: true
    color: Session.networkOnline ? theme.palette.normal.activity : theme.palette.normal.negative

    Behavior on height {
        NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
    }

    Label {
        anchors.centerIn: parent
        width: parent.width - units.gu(4)
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        color: "white"
        font.bold: true
        text: {
            if (!Session.networkOnline)
                return i18n.tr("Offline")
            if (Session.reconnectSeconds > 0)
                return i18n.tr("Reconnecting in %1 s — tap to retry").arg(Session.reconnectSeconds)
            return i18n.tr("Reconnecting…")
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: banner.shown && Session.networkOnline
        onClicked: Session.reconnect()
    }
}
