import QtQuick
import Lomiri.Components
import Disports.Core

ListItem {
    id: bubble

    required property string messageId
    required property string author
    required property string avatarUrl
    required property string body
    required property string plainBody
    required property string timestamp
    required property bool edited
    required property bool isPending
    required property bool isSystem
    required property bool grouped
    required property bool hasReply
    required property string replyAuthor
    required property string replyBody
    required property var attachments

    signal replyRequested(string messageId, string author)

    readonly property real avatarSize: units.gu(4.5)
    readonly property real contentLeft: units.gu(2) + avatarSize + units.gu(1.5)

    height: content.height + (grouped ? units.gu(0.3) : units.gu(1.2))
    divider.visible: false
    opacity: isPending ? 0.5 : 1

    trailingActions: ListItemActions {
        actions: [
            Action {
                iconName: "mail-reply"
                text: i18n.tr("Reply")
                visible: !bubble.isPending && !bubble.isSystem
                onTriggered: bubble.replyRequested(bubble.messageId, bubble.author)
            },
            Action {
                iconName: "edit-copy"
                text: i18n.tr("Copy")
                onTriggered: Clipboard.push(bubble.plainBody)
            }
        ]
    }

    SidebarIcon {
        id: avatar
        visible: !bubble.grouped && !bubble.isSystem
        anchors {
            left: parent.left
            top: parent.top
            leftMargin: units.gu(2)
            topMargin: units.gu(0.8)
        }
        width: bubble.avatarSize
        height: bubble.avatarSize
        imageSource: bubble.avatarUrl
    }

    Column {
        id: content
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: bubble.contentLeft
            rightMargin: units.gu(2)
            topMargin: bubble.grouped ? units.gu(0.15) : units.gu(0.8)
        }
        spacing: units.gu(0.3)

        // Reply citation
        Row {
            visible: bubble.hasReply
            width: parent.width
            spacing: units.gu(0.75)

            Rectangle {
                width: units.dp(2)
                height: replyLabel.height
                color: theme.palette.normal.base
            }

            Label {
                id: replyLabel
                width: parent.width - units.gu(1)
                text: "<b>" + bubble.replyAuthor + "</b> " + bubble.replyBody
                textFormat: Text.StyledText
                font.pixelSize: units.gu(1.4)
                color: theme.palette.normal.backgroundSecondaryText
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        // Author and time
        Row {
            visible: !bubble.grouped
            spacing: units.gu(1)

            Label {
                text: bubble.author
                font.pixelSize: units.gu(1.6)
                font.bold: true
                font.italic: bubble.isSystem
            }

            Label {
                anchors.baseline: parent.children[0].baseline
                text: bubble.timestamp
                font.pixelSize: units.gu(1.2)
                color: theme.palette.normal.backgroundSecondaryText
            }
        }

        Label {
            width: parent.width
            visible: text !== ""
            text: bubble.body + (bubble.edited ? " <font size=\"1\" color=\"#888\">(edited)</font>" : "")
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            font.pixelSize: units.gu(1.6)
            font.italic: bubble.isSystem
            color: bubble.isSystem ? theme.palette.normal.backgroundSecondaryText : theme.palette.normal.backgroundText
            onLinkActivated: function(link) { Qt.openUrlExternally(link) }
        }

        Repeater {
            model: bubble.attachments

            delegate: Item {
                required property var modelData

                readonly property real maxWidth: Math.min(content.width, units.gu(30))
                readonly property real ratio: modelData.width > 0 ? modelData.height / modelData.width : 0.75

                width: modelData.isImage ? Math.min(maxWidth, modelData.width > 0 ? modelData.width : maxWidth)
                                         : content.width
                height: modelData.isImage ? width * ratio : units.gu(4)

                Image {
                    anchors.fill: parent
                    visible: modelData.isImage
                    source: modelData.isImage ? modelData.url : ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    sourceSize.width: parent.width * 2
                    MouseArea {
                        anchors.fill: parent
                        onClicked: Qt.openUrlExternally(modelData.url)
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    visible: !modelData.isImage
                    radius: units.gu(0.5)
                    color: theme.palette.normal.base

                    Row {
                        anchors { left: parent.left; leftMargin: units.gu(1); verticalCenter: parent.verticalCenter }
                        spacing: units.gu(1)
                        Icon {
                            width: units.gu(2); height: width
                            name: "attachment"
                            color: theme.palette.normal.backgroundText
                        }
                        Label {
                            text: modelData.fileName
                            elide: Text.ElideMiddle
                            width: content.width - units.gu(5)
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: Qt.openUrlExternally(modelData.url)
                    }
                }
            }
        }
    }
}
