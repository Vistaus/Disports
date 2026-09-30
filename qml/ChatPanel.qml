import QtQuick
import QtQuick.Layouts
import Lomiri.Components
import Lomiri.Components.Popups
import Disports.Core

// Messages of the open channel plus the composer.
Item {
    id: chatPanel

    property bool showHeader: true
    property string replyToId: ""
    property string replyToAuthor: ""
    // Editing one of the user's messages: its id, and the text it had.
    property string editingId: ""
    property string editingOriginal: ""

    // Discord's limit for a message (without Nitro).
    readonly property int maxMessageLength: 2000

    signal infoRequested(string channelId)

    function startEditing(messageId, text) {
        closeEmoji()
        replyToId = ""
        editingId = messageId
        editingOriginal = text
        input.text = text
        input.forceActiveFocus()
        input.cursorPosition = input.length
    }

    function stopEditing() {
        if (editingId === "")
            return
        editingId = ""
        editingOriginal = ""
        input.text = ""
    }

    function confirmDelete(messageId) {
        PopupUtils.open(deleteDialog, chatPanel, { "messageId": messageId })
    }

    Component {
        id: deleteDialog

        Dialog {
            id: dialog
            property string messageId: ""
            title: i18n.tr("Delete message")
            text: i18n.tr("Are you sure you want to permanently delete this message?")

            Button {
                text: i18n.tr("Cancel")
                onClicked: PopupUtils.close(dialog)
            }

            Button {
                text: i18n.tr("Delete")
                color: theme.palette.normal.negative
                onClicked: {
                    Session.deleteMessage(dialog.messageId)
                    PopupUtils.close(dialog)
                }
            }
        }
    }

    // The emoji panel under the composer: "" (closed) or "compose".
    property string emojiMode: ""

    signal mediaOpened(var media)

    function openEmoji(mode) {
        emojiMode = mode
        emojiPicker.reset()
        Qt.inputMethod.hide()
        input.focus = false
    }

    function closeEmoji() {
        emojiMode = ""
    }

    function emojiPicked(emoji) {
        input.insert(input.cursorPosition, emoji.insertText)
    }

    // Reacting: the emoji picker in a popover by the message (or its +).
    function openReactionPicker(messageId, caller) {
        closeEmoji()
        PopupUtils.open(reactionPopover, caller, { "messageId": messageId })
    }

    Component {
        id: reactionPopover

        Popover {
            id: popover

            property string messageId: ""

            contentWidth: Math.min(units.gu(42), chatPanel.width - units.gu(4))

            Item {
                width: popover.contentWidth
                height: Math.min(units.gu(40), chatPanel.height * 0.6)

                EmojiPicker {
                    id: reactionPicker
                    anchors.fill: parent
                    color: "transparent"
                    Component.onCompleted: reset()
                    onPicked: function(emoji) {
                        Session.addReaction(popover.messageId, emoji.reaction)
                        PopupUtils.close(popover)
                    }
                }
            }
        }
    }

    // Opening an attachment closes the emoji panel.
    onMediaOpened: closeEmoji()

    function send() {
        const text = input.text
        if (text.trim() === "")
            return
        if (editingId !== "") {
            // Unchanged: nothing to send.
            if (text.trim() === editingOriginal.trim() || Session.editMessage(editingId, text))
                stopEditing()
            return
        }
        Session.sendMessage(text, chatPanel.replyToId)
        messageList.followNewest = true
        messageList.scrollToNewest()
        input.text = ""
        chatPanel.replyToId = ""
    }

    // Custom emoji in messages, sized with the text: a bit taller than a
    // line, and large for messages of only 1-3 emoji (like Discord).
    Binding {
        target: Session.messages
        property: "emojiSize"
        value: Math.round(units.gu(2.2))
    }

    Binding {
        target: Session.messages
        property: "jumboEmojiSize"
        value: Math.round(units.gu(5))
    }

    Connections {
        target: Session
        function onCurrentChannelChanged() {
            chatPanel.closeEmoji()
            chatPanel.replyToId = ""
            chatPanel.editingId = ""
            chatPanel.editingOriginal = ""
            input.text = ""
            messageList.followNewest = true
            messageList.scrollToNewest()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: theme.palette.normal.background
    }

    PanelHeader {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        visible: chatPanel.showHeader
        height: visible ? units.gu(5) : 0
        title: Session.currentChannelName
        subtitle: Session.currentChannelTopic
        actionIcon: "info"
        onActionTriggered: chatPanel.infoRequested(Session.currentChannelId)
    }

    ListView {
        id: messageList
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: typingLabel.top
        }
        clip: true
        model: Session.messages
        verticalLayoutDirection: ListView.BottomToTop
        cacheBuffer: units.gu(60)

        // Stay on the newest message until the user scrolls away, and go
        // back to it whenever the content grows (history arriving, images or
        // text laid out late) or the view shrinks (keyboard opening). With a
        // bottom-to-top list the newest message is at the visual bottom:
        // atYEnd, and positionViewAtBeginning() goes there.
        property bool followNewest: true

        function scrollToNewest() {
            Qt.callLater(function() {
                if (messageList.followNewest)
                    messageList.positionViewAtBeginning()
            })
        }

        onMovementEnded: followNewest = atYEnd
        onFlickEnded: followNewest = atYEnd
        onCountChanged: scrollToNewest()
        onContentHeightChanged: scrollToNewest()
        onHeightChanged: scrollToNewest()

        delegate: MessageDelegate {
            width: messageList.width
            onReplyRequested: function(messageId, author) {
                chatPanel.closeEmoji()
                chatPanel.stopEditing()
                chatPanel.replyToId = messageId
                chatPanel.replyToAuthor = author
                input.forceActiveFocus()
            }
            onReactRequested: function(messageId, caller) { chatPanel.openReactionPicker(messageId, caller) }
            onMediaOpened: function(media) { chatPanel.mediaOpened(media) }
            onEditRequested: function(messageId, text) { chatPanel.startEditing(messageId, text) }
            onDeleteRequested: function(messageId) { chatPanel.confirmDelete(messageId) }
        }

        // With a bottom-to-top view the footer sits above the oldest message.
        footer: Item {
            width: messageList.width
            height: units.gu(6)

            Button {
                anchors.centerIn: parent
                visible: Session.messages.hasOlder
                text: Session.loadingMessages ? i18n.tr("Loading…") : i18n.tr("Load older messages")
                enabled: !Session.loadingMessages
                onClicked: Session.loadOlderMessages()
            }

            Label {
                anchors.centerIn: parent
                visible: Session.messages.reachedStart && messageList.count > 0
                text: Session.inDirectMessages
                      ? i18n.tr("This is the beginning of your conversation with %1").arg(Session.currentChannelName)
                      : i18n.tr("This is the beginning of #%1").arg(Session.currentChannelName)
                color: theme.palette.normal.backgroundSecondaryText
            }
        }

        // Load older history automatically when scrolled to the top (the
        // visual top is atYBeginning in a bottom-to-top list).
        onAtYBeginningChanged: {
            if (atYBeginning && !followNewest && count > 0
                    && Session.messages.hasOlder && !Session.loadingMessages)
                Session.loadOlderMessages()
        }

        ActivityIndicator {
            anchors.centerIn: parent
            running: Session.loadingMessages && messageList.count === 0
        }
    }

    Label {
        id: typingLabel
        anchors {
            left: parent.left
            right: parent.right
            bottom: replyBar.top
            leftMargin: units.gu(2)
        }
        height: text !== "" ? units.gu(2.5) : 0
        text: Session.typingText
        font.pixelSize: units.gu(1.3)
        font.italic: true
        color: theme.palette.normal.backgroundSecondaryText
    }

    Rectangle {
        id: replyBar
        anchors { left: parent.left; right: parent.right; bottom: composer.top }
        height: chatPanel.replyToId !== "" || chatPanel.editingId !== "" ? units.gu(4) : 0
        visible: height > 0
        color: theme.palette.normal.base

        Label {
            anchors { left: parent.left; leftMargin: units.gu(2); verticalCenter: parent.verticalCenter }
            text: chatPanel.editingId !== "" ? i18n.tr("Editing message")
                                             : i18n.tr("Replying to %1").arg(chatPanel.replyToAuthor)
            font.pixelSize: units.gu(1.4)
        }

        Icon {
            anchors { right: parent.right; rightMargin: units.gu(2); verticalCenter: parent.verticalCenter }
            width: units.gu(2)
            height: width
            name: "close"
            color: theme.palette.normal.backgroundText
            MouseArea {
                anchors.fill: parent
                anchors.margins: -units.gu(1)
                onClicked: {
                    chatPanel.replyToId = ""
                    chatPanel.stopEditing()
                }
            }
        }
    }

    Rectangle {
        id: composer
        anchors { left: parent.left; right: parent.right; bottom: emojiPanel.top }
        height: Session.canSendMessages ? inputRow.implicitHeight + units.gu(2) : units.gu(5)
        color: theme.palette.normal.background

        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: units.dp(1)
            color: theme.palette.normal.base
        }

        Label {
            anchors.centerIn: parent
            visible: !Session.canSendMessages
            text: i18n.tr("You can't send messages in this channel")
            color: theme.palette.normal.backgroundSecondaryText
        }

        RowLayout {
            id: inputRow
            visible: Session.canSendMessages
            anchors {
                left: parent.left
                right: parent.right
                verticalCenter: parent.verticalCenter
                leftMargin: units.gu(1)
                rightMargin: units.gu(1)
            }
            spacing: units.gu(1)

            // Emoji panel / back to the keyboard
            AbstractButton {
                Layout.preferredWidth: units.gu(4.5)
                Layout.preferredHeight: units.gu(4.5)
                Layout.alignment: Qt.AlignBottom
                enabled: Session.connected
                opacity: enabled ? 1 : 0.4
                onClicked: {
                    if (chatPanel.emojiMode === "compose") {
                        chatPanel.closeEmoji()
                        input.forceActiveFocus()
                    } else {
                        chatPanel.openEmoji("compose")
                    }
                }

                Icon {
                    anchors.centerIn: parent
                    width: units.gu(2.8)
                    height: width
                    source: chatPanel.emojiMode === "compose" ? "" : "qrc:/assets/emoji.svg"
                    name: chatPanel.emojiMode === "compose" ? "input-keyboard-symbolic" : ""
                    color: theme.palette.normal.backgroundText
                }
            }

            // autoSize grows the TextArea's own height (its implicit height
            // stays one line), which RowLayout would override: let it size
            // itself here, and the row follow it.
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: input.height

                TextArea {
                    id: input
                    width: parent.width
                    autoSize: true
                    maximumLineCount: Session.preferences.composerMaxLines
                    placeholderText: Session.connected ? i18n.tr("Message %1").arg(Session.currentChannelName)
                                                       : i18n.tr("Waiting for connection…")
                    readOnly: !Session.connected
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    onTextChanged: {
                        if (length > chatPanel.maxMessageLength) {
                            // As the Qt 5 version: cut it, and say why.
                            text = text.substring(0, chatPanel.maxMessageLength)
                            cursorPosition = length
                            Session.showNotice(i18n.tr("Messages cannot exceed %1 characters. Your text has been truncated to fit the limit.")
                                               .arg(chatPanel.maxMessageLength))
                        }
                        if (text !== "" && chatPanel.editingId === "")
                            Session.notifyTyping()
                    }
                    onActiveFocusChanged: if (activeFocus && chatPanel.emojiMode === "compose") chatPanel.closeEmoji()
                    Keys.onReturnPressed: function(event) {
                        // Enter sends on a hardware keyboard; the on-screen
                        // keyboard's Enter inserts a line break.
                        if (Qt.inputMethod.visible || (event.modifiers & Qt.ShiftModifier)) {
                            event.accepted = false
                            return
                        }
                        chatPanel.send()
                        event.accepted = true
                    }
                }
            }

            AbstractButton {
                Layout.preferredWidth: units.gu(4.5)
                Layout.preferredHeight: units.gu(4.5)
                Layout.alignment: Qt.AlignBottom
                enabled: Session.connected && input.text.trim() !== ""
                opacity: enabled ? 1 : 0.4
                onClicked: chatPanel.send()

                Icon {
                    anchors.centerIn: parent
                    width: units.gu(3)
                    height: width
                    name: "send"
                    color: theme.palette.normal.backgroundText
                }
            }
        }
    }

    // Emoji picker below the composer, in place of the keyboard.
    Item {
        id: emojiPanel
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: chatPanel.emojiMode !== "" ? Math.min(units.gu(32), chatPanel.height * 0.5) : 0
        visible: height > 0
        clip: true

        Behavior on height {
            LomiriNumberAnimation {}
        }

        EmojiPicker {
            id: emojiPicker
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            onPicked: function(emoji) { chatPanel.emojiPicked(emoji) }
        }
    }
}
