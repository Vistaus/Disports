import QtQuick
import Lomiri.Components

Rectangle {
    id: panelHeader

    property string title: ""
    property string subtitle: ""
    // An icon button on the right, like a PageHeader action ("" for none).
    property string actionIcon: ""
    // A second one, left of it.
    property string secondActionIcon: ""
    property color secondActionColor: theme.palette.normal.backgroundText

    signal actionTriggered()
    signal secondActionTriggered()

    height: units.gu(5)
    clip: true
    color: theme.palette.normal.background

    Column {
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            leftMargin: units.gu(2)
            rightMargin: (panelHeader.actionIcon !== "" ? units.gu(6) : units.gu(2))
                         + (panelHeader.secondActionIcon !== "" ? units.gu(6) : 0)
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

    AbstractButton {
        visible: panelHeader.actionIcon !== ""
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: units.gu(6)
        onClicked: panelHeader.actionTriggered()

        Icon {
            anchors.centerIn: parent
            width: units.gu(2.5)
            height: width
            name: panelHeader.actionIcon
            color: theme.palette.normal.backgroundText
        }
    }

    AbstractButton {
        visible: panelHeader.secondActionIcon !== ""
        anchors { right: parent.right; rightMargin: units.gu(6); top: parent.top; bottom: parent.bottom }
        width: units.gu(6)
        onClicked: panelHeader.secondActionTriggered()

        Icon {
            anchors.centerIn: parent
            width: units.gu(2.5)
            height: width
            name: panelHeader.secondActionIcon
            color: panelHeader.secondActionColor
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: units.dp(1)
        color: theme.palette.normal.base
    }
}
