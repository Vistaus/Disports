import QtQuick
import Lomiri.Components
import Disports.Core

// Shown while a signed-in session is not connected, as in the Qt 5 version:
// an icon and what is going on ("Offline - showing saved conversations",
// "Reconnecting - retrying in 5 seconds"), then a short green "Connected"
// once it is back. It appears as soon as the connection drops and counts
// down live; tapping it retries right away.
Rectangle {
    id: banner

    readonly property bool reconnecting: Session.phase === SessionPhase.Ready && !Session.connected
    // Stays on while the success colour shows and while it closes.
    property bool showSuccess: false
    property bool successClosing: false
    readonly property bool expanded: reconnecting || showSuccess
    readonly property bool success: !reconnecting && (showSuccess || successClosing)

    height: expanded ? units.gu(4) : 0
    clip: true
    color: success ? theme.palette.normal.positive
         : Session.networkOnline ? theme.palette.normal.activity
         : theme.palette.normal.negative

    Behavior on height {
        NumberAnimation { duration: 250; easing.type: Easing.InOutQuad }
    }

    // Only a reconnect the user saw gets the "Connected" confirmation.
    property bool reconnectSeen: false
    onReconnectingChanged: {
        if (reconnecting) {
            reconnectSeen = true
            showSuccess = false
            successClosing = false
            successTimer.stop()
        } else if (reconnectSeen) {
            reconnectSeen = false
            showSuccess = true
            successTimer.restart()
        }
    }

    Timer {
        id: successTimer
        interval: 2000
        onTriggered: {
            banner.successClosing = true
            banner.showSuccess = false
            closedTimer.restart()
        }
    }

    Timer {
        id: closedTimer
        interval: 275
        onTriggered: banner.successClosing = false
    }

    Row {
        anchors.centerIn: parent
        width: Math.min(implicitWidth, parent.width - units.gu(4))
        spacing: units.gu(1)

        Icon {
            id: stateIcon
            anchors.verticalCenter: parent.verticalCenter
            width: units.gu(2)
            height: width
            name: banner.success ? "tick" : Session.networkOnline ? "sync" : "sync-error"
            color: "white"
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, banner.width - units.gu(4) - stateIcon.width - parent.spacing)
            elide: Text.ElideRight
            color: "white"
            font.bold: true
            text: {
                if (banner.success)
                    return i18n.tr("Connected")
                if (!Session.networkOnline)
                    return i18n.tr("Offline - showing saved conversations")
                if (Session.reconnectSeconds > 0)
                    return i18n.tr("Reconnecting - retrying in %1 second, tap to retry now",
                                   "Reconnecting - retrying in %1 seconds, tap to retry now",
                                   Session.reconnectSeconds).arg(Session.reconnectSeconds)
                return i18n.tr("Reconnecting - connecting to Discord")
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: banner.reconnecting && Session.networkOnline
        onClicked: Session.reconnect()
    }
}
