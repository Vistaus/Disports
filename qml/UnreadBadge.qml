import QtQuick
import Lomiri.Components

// A mention count, or a dot for unread messages without mentions.
Rectangle {
    id: badge

    property int mentions: 0
    property bool unread: false

    readonly property string kind: mentions > 0 ? "count" : (unread ? "dot" : "none")

    visible: kind !== "none"
    width: kind === "dot" ? units.gu(1.2) : Math.max(height, countLabel.width + units.gu(1.2))
    height: kind === "dot" ? units.gu(1.2) : units.gu(2.4)
    radius: height / 2
    color: theme.palette.normal.focus

    Label {
        id: countLabel
        anchors.centerIn: parent
        visible: badge.kind === "count"
        text: badge.mentions > 99 ? "99+" : String(badge.mentions)
        font.pixelSize: units.gu(1.3)
        font.bold: true
        color: "white"
    }
}
