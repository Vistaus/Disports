import QtQuick
import Lomiri.Components

Rectangle {
    id: panelHeader

    property string title: ""
    property string subtitle: ""

    height: units.gu(5)
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
            text: panelHeader.subtitle
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
