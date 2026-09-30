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
    required property bool isOwn
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
    required property bool jumbo
    required property bool separated
    required property bool blocked

    signal replyRequested(string messageId, string author)
    signal editRequested(string messageId, string text)
    signal deleteRequested(string messageId)
    // caller: the item the reaction picker points at
    signal reactRequested(string messageId, Item caller)
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

    readonly property bool showAvatar: Session.preferences.chatProfilePictures
    readonly property real avatarSize: units.gu(4.5)
    // Without pictures, system messages still keep room for their icon.
    readonly property real contentLeft: showAvatar ? units.gu(2) + avatarSize + units.gu(1.5)
                                        : (isSystem || placeholder) ? units.gu(5.5) : units.gu(2)

    // Messages from blocked users (Settings > Blocked messages): hidden, a
    // placeholder to tap, or shown.
    property bool revealed: false
    readonly property string blockedMode: blocked && !revealed ? Session.preferences.blockedMessages : "show"
    readonly property bool hiddenBlocked: blockedMode === "hide"
    readonly property bool placeholder: blockedMode === "reveal"

    // System lines get even room above and below; messages a bit more
    // above, where a new author starts.
    height: hiddenBlocked ? 0
          : placeholder ? placeholderLabel.height + units.gu(1.2)
          : isSystem ? content.height + units.gu(1.2)
          : grouped ? content.height + units.gu(0.3)
          // At least as tall as the avatar, so it never runs into the next row.
          : Math.max(content.height, avatar.visible ? avatar.height : 0) + units.gu(1.2)
    visible: !hiddenBlocked
    divider.visible: false
    opacity: isPending ? 0.5 : 1

    trailingActions: ListItemActions {
        actions: [
            Action {
                iconName: "mail-reply"
                text: i18n.tr("Reply")
                visible: !bubble.isPending && !bubble.isSystem && Session.canSendMessages
                onTriggered: bubble.replyRequested(bubble.messageId, bubble.author)
            },
            Action {
                iconName: "bot"
                text: i18n.tr("React")
                visible: !bubble.isPending && !bubble.isSystem && Session.canAddReactions
                onTriggered: bubble.reactRequested(bubble.messageId, bubble)
            },
            Action {
                iconName: "edit"
                text: i18n.tr("Edit")
                // Editing happens in the message box, there when sending is.
                visible: bubble.isOwn && !bubble.isPending && !bubble.isSystem && Session.canSendMessages
                onTriggered: bubble.editRequested(bubble.messageId, bubble.plainBody)
            },
            Action {
                iconName: "edit-copy"
                text: i18n.tr("Copy")
                onTriggered: Clipboard.push(bubble.plainBody)
            }
        ]
    }

    // Swiping the other way: delete, Lomiri's leading (destructive) action.
    // Own messages, or anyone's for moderators; not system messages (calls,
    // pins, joins).
    leadingActions: ListItemActions {
        actions: [
            Action {
                iconName: "delete"
                text: i18n.tr("Delete")
                visible: !bubble.isPending && !bubble.isSystem && (bubble.isOwn || Session.canManageMessages)
                onTriggered: bubble.deleteRequested(bubble.messageId)
            }
        ]
    }

    // A line above every message that is not grouped with the one above it
    // (another author, or the same one after a while).
    Rectangle {
        visible: bubble.separated
        anchors { top: parent.top; left: parent.left; right: parent.right; leftMargin: units.gu(2); rightMargin: units.gu(2) }
        height: units.dp(1)
        color: theme.palette.normal.base
    }

    // Rich text ignores linkColor; its links take their colour from CSS.
    readonly property string linkStyle: "<style>a { color: " + theme.palette.normal.activity + "; }</style>"

    // One-line rows: system messages and the placeholder of a blocked
    // user's message. An icon where the avatar goes, centred on the first
    // line of text.
    readonly property bool lineRow: isSystem || placeholder

    FontMetrics {
        id: bodyMetrics
        font: bodyLabel.font
    }

    Icon {
        visible: bubble.lineRow
        x: bubble.contentLeft - units.gu(1.5) - width
        // One line: its real height (inline emoji make it taller than the
        // font's line); more lines: the first one.
        readonly property Item line: bubble.placeholder ? placeholderLabel : bodyLabel
        readonly property real lineHeight: line.lineCount === 1 ? line.height : bodyMetrics.height
        y: (bubble.placeholder ? placeholderLabel.y : content.y + bodyLabel.y) + (lineHeight - height) / 2
        width: units.gu(2)
        height: width
        name: bubble.placeholder ? "security-alert" : bubble.systemIcon
        color: theme.palette.normal.backgroundSecondaryText
    }

    Label {
        id: placeholderLabel
        visible: bubble.placeholder
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: bubble.contentLeft
            rightMargin: units.gu(2)
            topMargin: units.gu(0.6)
        }
        text: bubble.linkStyle + i18n.tr("Message from a blocked user") + " · <a href=\"show\">" + i18n.tr("Show") + "</a>"
              + " <font size=\"1\" color=\"" + theme.palette.normal.backgroundSecondaryText + "\">" + bubble.timestamp + "</font>"
        textFormat: Text.RichText
        wrapMode: Text.Wrap
        font: bodyLabel.font
        color: theme.palette.normal.backgroundSecondaryText
        onLinkActivated: bubble.revealed = true
    }

    SidebarIcon {
        id: avatar
        visible: bubble.showAvatar && !bubble.grouped && !bubble.isSystem && !bubble.placeholder
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
        visible: !bubble.placeholder
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: bubble.contentLeft
            rightMargin: units.gu(2)
            topMargin: bubble.isSystem ? units.gu(0.6) : bubble.grouped ? units.gu(0.15) : units.gu(0.8)
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
            id: bodyLabel
            width: parent.width
            visible: bubble.body !== ""
            text: bubble.linkStyle + bubble.body + (bubble.isSystem
                                 ? " <font size=\"1\" color=\"" + theme.palette.normal.backgroundSecondaryText + "\">" + bubble.timestamp + "</font>"
                                 : bubble.edited ? " <font size=\"1\" color=\"#888\">(edited)</font>" : "")
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            // Only 1-3 emoji: large, like Discord
            font.pixelSize: bubble.jumbo ? units.gu(4.2) : units.gu(1.6)
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
            // System messages' reactions are shown, not joined in on.
            canAdd: !bubble.isSystem && Session.canAddReactions
            canToggle: !bubble.isSystem && Session.canUseReactions
            onAddRequested: function(caller) { bubble.reactRequested(bubble.messageId, caller) }
        }
    }
}
