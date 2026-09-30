import QtQuick
import Lomiri.Components
import Disports.Core

// While in a call away from the call screen: a strip to go back to it,
// like the dialer's "tap to return to call" bar.
Rectangle {
    id: banner

    property bool hidden: false
    readonly property bool shown: Session.call.state !== CallState.Idle && !hidden

    height: shown ? units.gu(4) : 0
    clip: true
    color: theme.palette.normal.positive

    Behavior on height {
        LomiriNumberAnimation {}
    }

    Row {
        anchors.centerIn: parent
        spacing: units.gu(1)

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            width: units.gu(2)
            height: width
            name: Session.call.state === CallState.Incoming ? "incoming-call" : "active-call"
            color: theme.palette.normal.positiveText
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, banner.width - units.gu(6))
            text: i18n.tr("Tap to return to call") + " · " + Session.call.statusText
            color: theme.palette.normal.positiveText
            elide: Text.ElideRight
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: Session.call.show()
    }
}
