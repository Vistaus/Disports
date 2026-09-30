import QtQuick
import QtQuick.Window
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

    // The on-screen keyboard holds the word being typed (predictive text)
    // outside `text` until it commits it. Commit before reading or changing
    // the text, or that word is lost (the Qt 5 version's fix, 80ca07f); and
    // reset the keyboard after replacing the whole text, so it does not
    // keep a stale word and put it back or drop letters later.
    function commitTyping() {
        Qt.inputMethod.commit()
    }

    function replaceText(text) {
        input.text = text
        Qt.inputMethod.reset()
    }

    signal infoRequested(string channelId)

    function startEditing(messageId, text) {
        closeEmoji()
        commitTyping()
        replyToId = ""
        editingId = messageId
        editingOriginal = text
        replaceText(text)
        input.forceActiveFocus()
        input.cursorPosition = input.length
    }

    function stopEditing() {
        if (editingId === "")
            return
        editingId = ""
        editingOriginal = ""
        replaceText("")
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
    // Choose a file to attach (the page holding the panel opens the picker
    // and calls attach()).
    signal attachRequested()

    // A file waiting to be sent with the next message.
    property string attachmentUrl: ""
    property string attachmentName: ""
    property var attachmentTransfer: null

    function attach(url, transfer) {
        clearAttachment()
        attachmentUrl = url
        attachmentName = decodeURIComponent(url.substring(url.lastIndexOf("/") + 1))
        attachmentTransfer = transfer
    }

    function clearAttachment() {
        // Content Hub's copy of the file is not needed any more.
        if (attachmentTransfer)
            attachmentTransfer.finalize()
        attachmentUrl = ""
        attachmentName = ""
        attachmentTransfer = null
    }

    // Mentions: the "@word" / "#word" being typed, and what it could be.
    property string mentionWord: ""
    property var suggestions: []

    function updateMention() {
        // With the word being typed (it sits at the cursor).
        const composing = input.inputMethodComposing ? input.displayText.substring(input.length) : ""
        const before = input.text.substring(0, input.cursorPosition) + composing
        const match = before.match(/(^|\s)([@#][^\s@#]*)$/)
        const word = match ? match[2] : ""
        if (word !== mentionWord) {
            mentionWord = word
            refreshSuggestions()
            // Server members not loaded yet: ask Discord (debounced).
            if (word.length > 1 && word[0] === "@")
                memberSearch.restart()
        }
    }

    function refreshSuggestions() {
        suggestions = mentionWord !== "" && Session.canSendMessages ? Session.mentionSuggestions(mentionWord) : []
    }

    function insertMention(text) {
        commitTyping()
        const end = input.cursorPosition
        const start = end - mentionWord.length
        input.remove(start, end)
        input.insert(start, text + " ")
        input.cursorPosition = start + text.length + 1
        mentionWord = ""
        suggestions = []
        input.forceActiveFocus()
    }

    Timer {
        id: memberSearch
        interval: 300
        onTriggered: Session.searchMembers(chatPanel.mentionWord.substring(1))
    }

    Connections {
        target: Session
        function onMembersChanged() { chatPanel.refreshSuggestions() }
        function onCurrentChannelChanged() {
            chatPanel.clearAttachment()
            chatPanel.mentionWord = ""
            chatPanel.suggestions = []
        }
    }

    // The message box's emoji picker is built when first opened.
    property bool emojiOpenedOnce: false

    function openEmoji(mode) {
        emojiOpenedOnce = true
        emojiMode = mode
        if (emojiLoader.item)
            emojiLoader.item.reset()
        Qt.inputMethod.hide()
        input.focus = false
    }

    function closeEmoji() {
        emojiMode = ""
    }

    function emojiPicked(emoji) {
        commitTyping()
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
        commitTyping()
        const text = input.text
        if (editingId === "" && attachmentUrl !== "") {
            if (Session.sendAttachment(attachmentUrl, text)) {
                clearAttachment()
                replaceText("")
                messageList.followNewest = true
                messageList.scrollToNewest()
            }
            return
        }
        if (text.trim() === "")
            return
        if (editingId !== "") {
            // Unchanged: nothing to send.
            if (text.trim() === editingOriginal.trim() || Session.editMessage(editingId, text))
                stopEditing()
            return
        }
        // Slowmode: keep the text until it may go.
        if (Session.slowmodeRemaining > 0) {
            Session.showNotice(i18n.tr("Slowmode is on: you can send another message in %1 s").arg(Session.slowmodeRemaining))
            return
        }
        Session.sendMessage(text, chatPanel.replyToId)
        messageList.followNewest = true
        messageList.scrollToNewest()
        replaceText("")
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
            chatPanel.replaceText("")
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
        secondActionIcon: !(Session.connected && Session.currentChannelId !== ""
                            && Session.call.canCall(Session.currentChannelId)) ? ""
                          : Session.currentChannelHasCall ? "active-call" : "call-start"
        secondActionColor: Session.currentChannelHasCall ? theme.palette.normal.positive
                                                         : theme.palette.normal.backgroundText
        onSecondActionTriggered: Session.call.start(Session.currentChannelId)
    }

    // A call going on here (that we are not in): how long, and tap to join.
    Rectangle {
        id: callBar
        readonly property bool inThisCall: Session.call.state !== CallState.Idle
                                          && Session.call.channelId === Session.currentChannelId
        anchors { top: header.bottom; left: parent.left; right: parent.right }
        height: Session.currentChannelHasCall && !inThisCall ? units.gu(4) : 0
        visible: height > 0
        clip: true
        color: theme.palette.normal.positive

        Row {
            anchors.centerIn: parent
            spacing: units.gu(1)

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                width: units.gu(2)
                height: width
                // (Suru's "active-call" is green already, and would not show.)
                name: "call-start"
                color: theme.palette.normal.positiveText
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: (Session.currentCallElapsed !== ""
                       ? i18n.tr("Call in progress · %1").arg(Session.currentCallElapsed)
                       : i18n.tr("Call in progress"))
                      + (Session.call.canCall(Session.currentChannelId) ? " · " + i18n.tr("Tap to join") : "")
                color: theme.palette.normal.positiveText
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: Session.call.start(Session.currentChannelId)
        }
    }

    ListView {
        id: messageList
        anchors {
            top: callBar.bottom
            left: parent.left
            right: parent.right
            bottom: typingLabel.top
        }
        clip: true
        model: Session.messages
        verticalLayoutDirection: ListView.BottomToTop

        // What part of the list is on screen, for the rows' onScreen (GIFs
        // playing): updated a few times a second while scrolling, not on
        // every frame.
        property real visibleTop: 0
        property real visibleBottom: 0
        function updateVisibleRange() {
            visibleTop = contentY
            visibleBottom = contentY + height
        }
        // Jumps made by code (to the newest message) are followed at once.
        onContentYChanged: if (!moving) updateVisibleRange()
        Component.onCompleted: updateVisibleRange()
        Timer {
            running: messageList.moving
            interval: 250
            repeat: true
            triggeredOnStart: true
            onTriggered: messageList.updateVisibleRange()
        }

        // Rows are built ahead of the finger, in the background; built on
        // the spot instead (when they come faster), each costs a frame.
        cacheBuffer: units.gu(150)

        // Scroll test (DISPORTS_TEST_SCROLL): flick towards older messages
        // every 1.5 s, as a finger would.
        property bool scrollTestStarted: false

        // Scroll test: frames drawn while the list moves, and the longest
        // wait between two of them, every second (idle time between flicks,
        // when nothing needs drawing, does not count).
        Connections {
            id: frameStats
            target: testScroll ? messageList.Window.window : null
            property real last: 0
            property int frames: 0
            property real worst: 0
            function onFrameSwapped() {
                const now = Date.now()
                if (messageList.moving && frameStats.last > 0)
                    frameStats.worst = Math.max(frameStats.worst, now - frameStats.last)
                if (messageList.moving)
                    frameStats.frames++
                frameStats.last = messageList.moving ? now : 0
            }
        }
        Timer {
            running: testScroll
            interval: 1000
            repeat: true
            onTriggered: {
                console.log("scroll-test: while moving: " + frameStats.frames + " frames, longest gap " + frameStats.worst + " ms")
                frameStats.frames = 0
                frameStats.worst = 0
            }
        }
        Timer {
            running: testScroll && messageList.count > 0
            interval: 1500
            repeat: true
            onTriggered: {
                messageList.scrollTestStarted = true
                messageList.flick(0, units.gu(250))
            }
        }

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

        onMovementEnded: {
            followNewest = atYEnd
            updateVisibleRange()
        }
        onFlickEnded: followNewest = atYEnd
        onCountChanged: scrollToNewest()
        onContentHeightChanged: {
            scrollToNewest()
            updateVisibleRange()
        }
        onHeightChanged: {
            scrollToNewest()
            updateVisibleRange()
        }

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

            // Without "Read Message History", only what arrives from now on.
            Label {
                anchors.centerIn: parent
                width: parent.width - units.gu(4)
                visible: Session.currentChannelId !== "" && !Session.canReadHistory
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: i18n.tr("You don't have permission to read earlier messages in this channel")
                color: theme.palette.normal.backgroundSecondaryText
            }

            Button {
                anchors.centerIn: parent
                visible: Session.messages.hasOlder && Session.canReadHistory
                text: Session.loadingMessages ? i18n.tr("Loading…") : i18n.tr("Load older messages")
                enabled: !Session.loadingMessages
                onClicked: Session.loadOlderMessages()
            }

            Label {
                anchors.centerIn: parent
                visible: Session.messages.reachedStart && messageList.count > 0 && Session.canReadHistory
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

    // Who or what an @ / # being typed could be.
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: replyBar.top }
        height: Math.min(suggestionList.contentHeight, units.gu(30))
        visible: chatPanel.suggestions.length > 0
        z: 3
        color: theme.palette.normal.background

        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: units.dp(1)
            color: theme.palette.normal.base
        }

        ListView {
            id: suggestionList
            anchors.fill: parent
            clip: true
            model: chatPanel.suggestions

            delegate: ListItem {
                required property var modelData
                height: suggestionLayout.height + (divider.visible ? divider.height : 0)
                onClicked: chatPanel.insertMention(modelData.insert)

                ListItemLayout {
                    id: suggestionLayout
                    title.text: modelData.label
                    title.color: modelData.color ? modelData.color : theme.palette.normal.backgroundText
                    subtitle.text: modelData.detail

                    Item {
                        SlotsLayout.position: SlotsLayout.Leading
                        width: units.gu(4)
                        height: width

                        LomiriShape {
                            anchors.fill: parent
                            visible: modelData.kind === "user"
                            aspect: LomiriShape.Flat
                            backgroundColor: theme.palette.normal.base
                            sourceFillMode: LomiriShape.PreserveAspectCrop
                            source: Image {
                                source: modelData.kind === "user" ? modelData.avatarUrl : ""
                                sourceSize.width: units.gu(8)
                                sourceSize.height: units.gu(8)
                                asynchronous: true
                            }
                        }

                        Icon {
                            anchors.centerIn: parent
                            visible: modelData.kind !== "user"
                            width: units.gu(2.5)
                            height: width
                            name: modelData.kind === "channel" ? "message"
                                : modelData.kind === "everyone" ? "notification"
                                : "contact-group"
                            color: theme.palette.normal.backgroundSecondaryText
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: replyBar
        anchors { left: parent.left; right: parent.right; bottom: attachBar.top }
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

    // The file going with the next message, or being sent.
    Rectangle {
        id: attachBar
        anchors { left: parent.left; right: parent.right; bottom: composer.top }
        height: chatPanel.attachmentUrl !== "" || Session.uploading ? units.gu(5) : 0
        visible: height > 0
        color: theme.palette.normal.base

        Icon {
            id: attachIcon
            anchors { left: parent.left; leftMargin: units.gu(2); verticalCenter: parent.verticalCenter }
            width: units.gu(2.5)
            height: width
            name: "attachment"
            color: theme.palette.normal.backgroundText
        }

        Column {
            anchors {
                left: attachIcon.right
                right: attachClose.left
                leftMargin: units.gu(1.5)
                rightMargin: units.gu(1.5)
                verticalCenter: parent.verticalCenter
            }
            spacing: units.gu(0.5)

            Label {
                width: parent.width
                text: Session.uploading ? i18n.tr("Sending %1").arg(Session.uploadName) : chatPanel.attachmentName
                font.pixelSize: units.gu(1.4)
                elide: Text.ElideMiddle
            }

            ProgressBar {
                width: parent.width
                height: units.gu(0.5)
                visible: Session.uploading
                minimumValue: 0
                maximumValue: 1
                value: Session.uploadProgress
                showProgressPercentage: false
            }
        }

        Icon {
            id: attachClose
            anchors { right: parent.right; rightMargin: units.gu(2); verticalCenter: parent.verticalCenter }
            width: units.gu(2)
            height: width
            name: "close"
            color: theme.palette.normal.backgroundText
            MouseArea {
                anchors.fill: parent
                anchors.margins: -units.gu(1)
                onClicked: Session.uploading ? Session.cancelUpload() : chatPanel.clearAttachment()
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
            text: Session.timeoutText !== "" ? Session.timeoutText
                                              : i18n.tr("You can't send messages in this channel")
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

            // Attach a file (Content Hub)
            AbstractButton {
                Layout.preferredWidth: units.gu(4.5)
                Layout.preferredHeight: units.gu(4.5)
                Layout.alignment: Qt.AlignBottom
                visible: Session.canAttachFiles && chatPanel.editingId === ""
                enabled: Session.connected && !Session.uploading
                opacity: enabled ? 1 : 0.4
                onClicked: {
                    chatPanel.closeEmoji()
                    chatPanel.attachRequested()
                }

                Icon {
                    anchors.centerIn: parent
                    width: units.gu(2.8)
                    height: width
                    name: "attachment"
                    color: theme.palette.normal.backgroundText
                }
            }

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
                    // Suru's "bot", as for reactions
                    name: chatPanel.emojiMode === "compose" ? "input-keyboard-symbolic" : "bot"
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
                    placeholderText: !Session.connected ? i18n.tr("Waiting for connection…")
                                   : Session.slowmodeRemaining > 0 ? i18n.tr("Slowmode: wait %1 s").arg(Session.slowmodeRemaining)
                                   : Session.slowmodeSeconds > 0 ? i18n.tr("Message %1 (slowmode: %2 s)").arg(Session.currentChannelName).arg(Session.slowmodeSeconds)
                                   : i18n.tr("Message %1").arg(Session.currentChannelName)
                    readOnly: !Session.connected
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    onTextChanged: {
                        if (length > chatPanel.maxMessageLength) {
                            // As the Qt 5 version: cut it, and say why.
                            chatPanel.commitTyping()
                            chatPanel.replaceText(text.substring(0, chatPanel.maxMessageLength))
                            cursorPosition = length
                            Session.showNotice(i18n.tr("Messages cannot exceed %1 characters. Your text has been truncated to fit the limit.")
                                               .arg(chatPanel.maxMessageLength))
                        }
                        chatPanel.updateMention()
                    }
                    // Also while a word is being typed (not committed yet).
                    onDisplayTextChanged: {
                        if (displayText !== "" && chatPanel.editingId === "")
                            Session.notifyTyping()
                        chatPanel.updateMention()
                    }
                    onCursorPositionChanged: chatPanel.updateMention()
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
                enabled: Session.connected
                         && (input.displayText.trim() !== "" || (chatPanel.attachmentUrl !== "" && chatPanel.editingId === ""))
                         && (chatPanel.editingId !== "" || Session.slowmodeRemaining === 0)
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

        // Built the first time it opens (hundreds of emoji), then kept.
        Loader {
            id: emojiLoader
            anchors.fill: parent
            active: chatPanel.emojiOpenedOnce
            sourceComponent: EmojiPicker {
                onPicked: function(emoji) { chatPanel.emojiPicked(emoji) }
            }
        }
    }
}
