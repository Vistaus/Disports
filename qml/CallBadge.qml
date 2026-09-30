import QtQuick
import Lomiri.Components

// A call going on (servers, conversations): Suru's phone on a green dot,
// ringed with the background so it stands off what it sits on.
Rectangle {
    width: units.gu(2.2)
    height: width
    radius: width / 2
    color: theme.palette.normal.positive
    border.width: units.dp(2)
    border.color: theme.palette.normal.background

    Icon {
        anchors.centerIn: parent
        width: parent.width * 0.55
        height: width
        name: "call-start"
        color: "white"
    }
}
