import QtQuick
import Lomiri.Components

Rectangle {
    id: panelHeader

    property string title: ""
    property string subtitle: ""

    height: units.gu(5)
    clip: true
    color: theme.palette.normal.background

    Column {
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            leftMargin: units.gu(2)
            rightMargin: units.gu(2)
        }

        Label {
            width: parent.width
            text: panelHeader.title
            font.pixelSize: units.gu(1.8)
            font.bold: true
            elide: Text.ElideRight
        }

        Label {
            width: parent.width
            visible: panelHeader.subtitle !== ""
            // Channel topics can have several lines; one line fits here.
            text: panelHeader.subtitle.replace(/\s*\n+\s*/g, "  ")
            wrapMode: Text.NoWrap
            font.pixelSize: units.gu(1.3)
            color: theme.palette.normal.backgroundSecondaryText
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: units.dp(1)
        color: theme.palette.normal.base
    }
}
