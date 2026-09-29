import QtQuick
import Lomiri.Components
import Disports.Core

// Emoji picker: search, categories down the side (the server's own emoji
// first) and a grid. Used below the composer, and in a popover to react.
Rectangle {
    id: picker

    // An entry of Session.emoji: {reaction, insertText, name}
    signal picked(var emoji)

    color: theme.palette.normal.background

    function reset() {
        searchField.text = ""
        if (Session.emoji.hasServerEmoji && !Session.inDirectMessages)
            Session.emoji.category = "server"
        else if (Session.emoji.category === "server")
            Session.emoji.category = "faces"
    }

    Rectangle {
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: units.dp(1)
        color: theme.palette.normal.base
    }

    TextField {
        id: searchField
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: units.gu(1)
        }
        placeholderText: i18n.tr("Search emoji")
        inputMethodHints: Qt.ImhNoPredictiveText
        onTextChanged: Session.emoji.search = text
    }

    // Categories down the side, so the grid keeps the height. The current
    // one is marked like Lomiri's Sections, with a bar in the accent colour.
    ListView {
        id: categoryBar
        anchors {
            top: searchField.bottom
            left: parent.left
            bottom: parent.bottom
            topMargin: units.gu(0.5)
        }
        width: visible ? units.gu(5) : 0
        clip: true
        visible: searchField.text === ""
        model: Session.emoji.categories

        delegate: AbstractButton {
            id: categoryButton

            required property var modelData

            readonly property bool isServer: modelData.key === "server"
            readonly property bool current: Session.emoji.category === modelData.key

            width: categoryBar.width
            height: visible ? units.gu(4.5) : 0
            visible: !isServer || Session.emoji.hasServerEmoji
            onClicked: Session.emoji.category = modelData.key

            Rectangle {
                anchors.fill: parent
                visible: categoryButton.pressed
                color: theme.palette.highlighted.background
            }

            Rectangle {
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: units.gu(0.75) }
                anchors.leftMargin: 0
                width: units.dp(3)
                visible: categoryButton.current
                color: theme.palette.normal.activity
            }

            Icon {
                anchors.centerIn: parent
                visible: categoryButton.isServer
                width: units.gu(2.5)
                height: width
                name: "contact-group"
                color: categoryButton.current ? theme.palette.normal.backgroundText
                                              : theme.palette.normal.backgroundSecondaryText
            }

            Label {
                anchors.centerIn: parent
                visible: !categoryButton.isServer
                text: categoryButton.modelData.icon
                font.pixelSize: units.gu(2.2)
                opacity: categoryButton.current ? 1 : 0.6
            }
        }
    }

    Rectangle {
        visible: categoryBar.visible
        anchors { top: categoryBar.top; bottom: parent.bottom; left: categoryBar.right }
        width: units.dp(1)
        color: theme.palette.normal.base
    }

    GridView {
        id: grid
        anchors {
            top: searchField.bottom
            left: categoryBar.right
            right: parent.right
            bottom: parent.bottom
            margins: units.gu(0.5)
        }
        clip: true
        model: Session.emoji
        cellWidth: width / Math.max(1, Math.floor(width / units.gu(5)))
        cellHeight: units.gu(5)
        cacheBuffer: units.gu(40)

        delegate: AbstractButton {
            id: cell

            required property bool isCustom
            required property string text
            required property string imageUrl
            required property string name
            required property string reaction
            required property string insertText

            width: grid.cellWidth
            height: grid.cellHeight
            onClicked: picker.picked({ "reaction": reaction, "insertText": insertText, "name": name })

            Rectangle {
                anchors.fill: parent
                anchors.margins: units.dp(2)
                radius: units.gu(0.6)
                color: theme.palette.highlighted.base
                visible: cell.pressed
            }

            Label {
                anchors.centerIn: parent
                visible: !cell.isCustom
                text: cell.text
                font.pixelSize: units.gu(2.6)
            }

            Image {
                anchors.centerIn: parent
                visible: cell.isCustom
                width: units.gu(3.2)
                height: width
                source: cell.isCustom ? cell.imageUrl : ""
                sourceSize.width: units.gu(6)
                sourceSize.height: units.gu(6)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
        }

        Label {
            anchors.centerIn: parent
            visible: grid.count === 0
            text: i18n.tr("No emoji found")
            color: theme.palette.normal.backgroundSecondaryText
        }
    }
}
