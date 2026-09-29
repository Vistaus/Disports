import QtQuick
import Lomiri.Components
import Disports.Core

// Emoji picker: search, a category bar (the server's own emoji first) and a
// grid. Used for the composer and for reacting to messages.
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

    ListView {
        id: categoryBar
        anchors {
            top: searchField.bottom
            left: parent.left
            right: parent.right
            topMargin: units.gu(0.5)
        }
        height: units.gu(4.5)
        orientation: ListView.Horizontal
        clip: true
        visible: searchField.text === ""
        model: Session.emoji.categories

        delegate: AbstractButton {
            id: categoryButton

            required property var modelData

            readonly property bool isServer: modelData.key === "server"
            readonly property bool current: Session.emoji.category === modelData.key

            width: visible ? units.gu(4.5) : 0
            height: categoryBar.height
            visible: !isServer || Session.emoji.hasServerEmoji
            onClicked: Session.emoji.category = modelData.key

            Rectangle {
                anchors.fill: parent
                anchors.margins: units.dp(2)
                radius: units.gu(0.6)
                color: categoryButton.current ? theme.palette.highlighted.base : "transparent"
            }

            Icon {
                anchors.centerIn: parent
                visible: categoryButton.isServer
                width: units.gu(2.5)
                height: width
                name: "contact-group"
                color: theme.palette.normal.backgroundText
            }

            Label {
                anchors.centerIn: parent
                visible: !categoryButton.isServer
                text: categoryButton.modelData.icon
                font.pixelSize: units.gu(2.2)
            }
        }
    }

    GridView {
        id: grid
        anchors {
            top: categoryBar.visible ? categoryBar.bottom : searchField.bottom
            left: parent.left
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
