import QtQuick
import Lomiri.Components

// A mention count bubble, or a small dot for unread messages.
Rectangle {
    id: badge

    property int mentions: 0
    property bool unread: false

    visible: mentions > 0 || unread
    width: mentions > 0 ? Math.max(height, countLabel.implicitWidth + units.gu(1)) : units.gu(1)
    height: mentions > 0 ? units.gu(2.2) : units.gu(1)
    radius: height / 2
    color: mentions > 0 ? theme.palette.normal.negative : theme.palette.normal.backgroundText

    Label {
        id: countLabel
        anchors.centerIn: parent
        visible: badge.mentions > 0
        text: badge.mentions > 99 ? "99+" : String(badge.mentions)
        font.pixelSize: units.gu(1.3)
        font.bold: true
        color: "white"
    }
}
