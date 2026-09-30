import QtQuick
import Lomiri.Components
import Disports.Core

// Someone's call in the open conversation that we aren't in: how long it
// has been going, and tap to join.
Rectangle {
    readonly property bool inThisCall: Session.call.state !== CallState.Idle
                                      && Session.call.channelId === Session.currentChannelId
    readonly property bool canJoin: Session.call.canCall(Session.currentChannelId)

    height: Session.currentChannelHasCall && !inThisCall ? units.gu(4) : 0
    visible: height > 0
    clip: true
    color: theme.palette.normal.positive

    Row {
        anchors.centerIn: parent
        spacing: units.gu(1)

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            width: units.gu(2)
            height: width
            // Suru's "active-call" is green already.
            name: "call-start"
            color: theme.palette.normal.positiveText
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: (Session.currentCallElapsed !== ""
                   ? i18n.tr("Call in progress · %1").arg(Session.currentCallElapsed)
                   : i18n.tr("Call in progress"))
                  + (canJoin ? " · " + i18n.tr("Tap to join") : "")
            color: theme.palette.normal.positiveText
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: Session.call.start(Session.currentChannelId)
    }
}
