import QtQuick
import Lomiri.Components

// A quoted message: a coloured bar, a title in the same colour, and a line
// of text. Replies in the chat, and replying or editing over the composer.
Item {
    id: citation

    property string title
    property string text
    property color accentColor: theme.palette.normal.activity
    // Replies: tapping goes to the message.
    property bool tappable: false

    signal clicked()

    implicitHeight: column.height

    Rectangle {
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: units.dp(3)
        radius: units.dp(2)
        color: citation.accentColor
    }

    Column {
        id: column
        anchors { left: parent.left; right: parent.right; leftMargin: units.gu(1) }

        Label {
            width: parent.width
            text: citation.title
            font.bold: true
            font.pixelSize: units.gu(1.4)
            color: citation.accentColor
            elide: Text.ElideRight
        }

        Label {
            width: parent.width
            visible: text !== ""
            text: citation.text
            font.pixelSize: units.gu(1.4)
            color: theme.palette.normal.backgroundSecondaryText
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: citation.tappable
        onClicked: citation.clicked()
    }
}
