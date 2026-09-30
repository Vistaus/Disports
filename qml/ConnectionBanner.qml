import QtQuick
import Lomiri.Components
import Disports.Core

// Shown while signed in but not connected ("Reconnecting - retrying in 5
// seconds"), then briefly "Connected" once back. Tap to retry now.
Rectangle {
    id: banner

    readonly property bool reconnecting: Session.phase === SessionPhase.Ready && !Session.connection.connected
    // Stays on while the success colour shows and while it closes.
    property bool showSuccess: false
    property bool successClosing: false
    readonly property bool expanded: reconnecting || showSuccess
    readonly property bool success: !reconnecting && (showSuccess || successClosing)

    height: expanded ? units.gu(4) : 0
    clip: true
    color: success ? theme.palette.normal.positive
         : Session.connection.networkOnline ? theme.palette.normal.activity
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
            name: banner.success ? "tick" : Session.connection.networkOnline ? "sync" : "sync-error"
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
                if (!Session.connection.networkOnline)
                    return i18n.tr("Offline - showing saved conversations")
                if (Session.connection.reconnectSeconds > 0)
                    return i18n.tr("Reconnecting - retrying in %1 second, tap to retry now",
                                   "Reconnecting - retrying in %1 seconds, tap to retry now",
                                   Session.connection.reconnectSeconds).arg(Session.connection.reconnectSeconds)
                return i18n.tr("Reconnecting - connecting to Discord")
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: banner.reconnecting && Session.connection.networkOnline
        onClicked: Session.connection.reconnect()
    }
}
