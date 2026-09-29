import QtQuick
import Lomiri.Components
import Disports.Core

ListItem {
    id: bubble

    required property int index
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
    required property var media
    required property var reactions
    required property var embeds
    required property string systemIcon
    required property string interaction
    required property bool forwarded
    required property var stickers
    required property var poll

    signal replyRequested(string messageId, string author)
    signal reactRequested(string messageId)
    signal mediaOpened(var media)

    // On screen with the app not hidden or suspended: GIFs may play (if
    // enabled).
    readonly property bool onScreen: {
        const view = ListView.view
        return view !== null && visible
               && Qt.application.state !== Qt.ApplicationHidden
               && Qt.application.state !== Qt.ApplicationSuspended
               && y + height > view.contentY && y < view.contentY + view.height
    }

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
                iconName: "add"
                text: i18n.tr("React")
                visible: !bubble.isPending && !bubble.isSystem
                onTriggered: bubble.reactRequested(bubble.messageId)
            },
            Action {
                iconName: "edit-copy"
                text: i18n.tr("Copy")
                onTriggered: Clipboard.push(bubble.plainBody)
            }
        ]
    }

    // System messages (joins, pins, calls, ...): an icon where the avatar
    // goes, and one line of text that names the author.
    Icon {
        visible: bubble.isSystem
        anchors {
            right: content.left
            rightMargin: units.gu(1.5)
            top: parent.top
            topMargin: units.gu(0.9)
        }
        width: units.gu(2.2)
        height: width
        name: bubble.systemIcon
        color: theme.palette.normal.backgroundSecondaryText
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

        // Who ran the command this message answers
        Row {
            visible: bubble.interaction !== ""
            width: parent.width
            spacing: units.gu(0.75)

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                width: units.gu(1.6)
                height: width
                name: "stock_application"
                color: theme.palette.normal.backgroundSecondaryText
            }

            Label {
                width: parent.width - units.gu(2.5)
                text: bubble.interaction
                textFormat: Text.StyledText
                textSize: Label.Small
                color: theme.palette.normal.backgroundSecondaryText
                elide: Text.ElideRight
            }
        }

        // Author and time
        Row {
            visible: !bubble.grouped && !bubble.isSystem
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

        Row {
            visible: bubble.forwarded
            spacing: units.gu(0.75)

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                width: units.gu(1.6)
                height: width
                name: "mail-forwarded"
                color: theme.palette.normal.backgroundSecondaryText
            }

            Label {
                text: i18n.tr("Forwarded")
                textSize: Label.Small
                font.italic: true
                color: theme.palette.normal.backgroundSecondaryText
            }
        }

        Label {
            width: parent.width
            visible: text !== ""
            text: bubble.body + (bubble.isSystem
                                 ? " <font size=\"1\" color=\"" + theme.palette.normal.backgroundSecondaryText + "\">" + bubble.timestamp + "</font>"
                                 : bubble.edited ? " <font size=\"1\" color=\"#888\">(edited)</font>" : "")
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            font.pixelSize: units.gu(1.6)
            color: bubble.isSystem ? theme.palette.normal.backgroundSecondaryText : theme.palette.normal.backgroundText
            onLinkActivated: function(link) { Qt.openUrlExternally(link) }
        }

        Repeater {
            model: bubble.media

            delegate: MediaPreview {
                required property var modelData
                media: modelData
                maxWidth: Math.min(content.width, units.gu(30))
                playing: bubble.onScreen
                onOpened: function(media) { bubble.mediaOpened(media) }
            }
        }

        Repeater {
            model: bubble.embeds

            delegate: EmbedCard {
                required property var modelData
                embed: modelData
                width: Math.min(content.width, units.gu(52))
                playing: bubble.onScreen
                onMediaOpened: function(media) { bubble.mediaOpened(media) }
            }
        }

        Repeater {
            model: bubble.stickers

            delegate: StickerView {
                required property var modelData
                sticker: modelData
                playing: bubble.onScreen
            }
        }

        Loader {
            active: !!bubble.poll
            visible: active
            width: Math.min(content.width, units.gu(52))
            sourceComponent: PollCard {
                messageId: bubble.messageId
                poll: bubble.poll
            }
        }

        ReactionBar {
            width: parent.width
            visible: bubble.reactions.length > 0
            messageId: bubble.messageId
            reactions: bubble.reactions
            onAddRequested: bubble.reactRequested(bubble.messageId)
        }
    }
}
