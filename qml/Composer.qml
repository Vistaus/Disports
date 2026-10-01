import QtQuick
import QtQuick.Layouts
import Lomiri.Components
import Disports.Core

// The message box: writing, replying, editing, attaching a file and
// @ / # suggestions.
Item {
    id: composer

    implicitHeight: content.height

    // Discord's limit without Nitro.
    readonly property int maxMessageLength: 2000

    property string replyToId: ""
    property string replyToAuthor: ""
    property string replyToText: ""
    property string editingId: ""
    property string editingOriginal: ""
    property string attachmentUrl: ""
    property string attachmentName: ""
    property var attachmentTransfer: null
    property string mentionWord: ""
    property var suggestions: []

    // Shown in place of the keyboard, below the composer (ChatPanel).
    property bool emojiOpen: false

    signal attachRequested()
    signal emojiButtonClicked()
    signal inputFocused()
    signal sent()

    // The on-screen keyboard keeps the word being typed (predictive text)
    // out of `text` until it commits it: commit before reading or changing
    // the text, or the word is lost. After replacing the whole text, reset
    // the keyboard so it doesn't put a stale word back.
    function commitTyping() {
        Qt.inputMethod.commit()
    }

    function replaceText(text) {
        input.text = text
        Qt.inputMethod.reset()
    }

    function focusInput() {
        input.forceActiveFocus()
    }

    function hideKeyboard() {
        Qt.inputMethod.hide()
        input.focus = false
    }

    function insertText(text) {
        commitTyping()
        input.insert(input.cursorPosition, text)
    }

    function startReply(messageId, author, text) {
        stopEditing()
        replyToId = messageId
        replyToAuthor = author
        replyToText = text
        focusInput()
    }

    function startEditing(messageId, text) {
        commitTyping()
        replyToId = ""
        editingId = messageId
        editingOriginal = text
        replaceText(text)
        focusInput()
        input.cursorPosition = input.length
    }

    function stopEditing() {
        if (editingId === "")
            return
        editingId = ""
        editingOriginal = ""
        replaceText("")
    }

    function attach(url, transfer) {
        clearAttachment()
        attachmentUrl = url
        attachmentName = decodeURIComponent(url.substring(url.lastIndexOf("/") + 1))
        attachmentTransfer = transfer
    }

    function clearAttachment() {
        // Content Hub's copy of the file isn't needed any more.
        if (attachmentTransfer)
            attachmentTransfer.finalize()
        attachmentUrl = ""
        attachmentName = ""
        attachmentTransfer = null
    }

    function reset() {
        replyToId = ""
        editingId = ""
        editingOriginal = ""
        mentionWord = ""
        suggestions = []
        clearAttachment()
        replaceText("")
    }

    function send() {
        commitTyping()
        const text = input.text
        if (editingId !== "") {
            // Unchanged: nothing to send.
            if (text.trim() !== "" && (text.trim() === editingOriginal.trim() || Session.editMessage(editingId, text)))
                stopEditing()
            return
        }
        if (attachmentUrl !== "") {
            if (Session.sender.sendFile(attachmentUrl, text)) {
                clearAttachment()
                replaceText("")
                sent()
            }
            return
        }
        if (text.trim() === "")
            return
        // Keep the text until slowmode lets it go.
        if (Session.sender.slowmodeRemaining > 0) {
            Session.showNotice(i18n.tr("Slowmode: wait %1 s").arg(Session.sender.slowmodeRemaining))
            return
        }
        Session.sender.send(text, replyToId)
        replaceText("")
        replyToId = ""
        sent()
    }

    // The @word or #word at the cursor, including a word the keyboard is
    // still composing.
    function updateMention() {
        const composing = input.inputMethodComposing ? input.displayText.substring(input.length) : ""
        const before = input.text.substring(0, input.cursorPosition) + composing
        const match = before.match(/(^|\s)([@#][^\s@#]*)$/)
        const word = match ? match[2] : ""
        if (word === mentionWord)
            return
        mentionWord = word
        refreshSuggestions()
        // Server members that aren't loaded yet are searched for.
        if (word.length > 1 && word[0] === "@")
            memberSearch.restart()
    }

    function refreshSuggestions() {
        suggestions = mentionWord !== "" && Session.permissions.canSendMessages
                      ? Session.mentions.suggestions(mentionWord) : []
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
        focusInput()
    }

    Timer {
        id: memberSearch
        interval: 300
        onTriggered: Session.mentions.searchMembers(composer.mentionWord.substring(1))
    }

    // New members found by the search.
    Connections {
        target: Session
        function onMembersChanged() { composer.refreshSuggestions() }
    }

    MentionPopup {
        anchors.bottom: parent.top
        width: parent.width
        z: 3
        suggestions: composer.suggestions
        onPicked: function(text) { composer.insertMention(text) }
    }

    Column {
        id: content
        width: parent.width

        // Replying or editing: the message, quoted.
        Item {
            width: parent.width
            height: composer.replyToId !== "" || composer.editingId !== "" ? quote.height + units.gu(1.5) : 0
            visible: height > 0

            MessageCitation {
                id: quote
                anchors { left: parent.left; right: parent.right; leftMargin: units.gu(2); rightMargin: units.gu(5)
                          verticalCenter: parent.verticalCenter }
                accentColor: composer.editingId !== "" ? "#F0B232" : theme.palette.normal.activity
                title: composer.editingId !== "" ? i18n.tr("Editing message")
                                                 : i18n.tr("Replying to %1").arg(composer.replyToAuthor)
                text: composer.editingId !== "" ? composer.editingOriginal : composer.replyToText
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
                        composer.replyToId = ""
                        composer.stopEditing()
                    }
                }
            }
        }

        AttachmentBar {
            width: parent.width
            fileName: composer.attachmentName
            onClosed: composer.clearAttachment()
        }

        Rectangle {
            width: parent.width
            height: Session.permissions.canSendMessages ? inputRow.implicitHeight + units.gu(2) : units.gu(5)
            color: theme.palette.normal.background

            Rectangle {
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: units.dp(1)
                color: theme.palette.normal.base
            }

            Label {
                anchors.centerIn: parent
                visible: !Session.permissions.canSendMessages
                text: Session.permissions.timeoutText !== "" ? Session.permissions.timeoutText
                                                             : i18n.tr("You can't send messages in this channel")
                color: theme.palette.normal.backgroundSecondaryText
            }

            RowLayout {
                id: inputRow
                visible: Session.permissions.canSendMessages
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    leftMargin: units.gu(1)
                    rightMargin: units.gu(1)
                }
                spacing: units.gu(1)

                ComposerButton {
                    iconName: "attachment"
                    visible: Session.permissions.canAttachFiles && composer.editingId === ""
                    enabled: Session.connection.connected && !Session.sender.uploading
                    onClicked: composer.attachRequested()
                }

                ComposerButton {
                    iconName: composer.emojiOpen ? "input-keyboard-symbolic" : "bot"
                    enabled: Session.connection.connected
                    onClicked: composer.emojiButtonClicked()
                }

                // autoSize grows the TextArea's height but not its implicit
                // height, which RowLayout would use: size it here instead.
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: input.height

                    TextArea {
                        id: input
                        width: parent.width
                        autoSize: true
                        maximumLineCount: Session.preferences.composerMaxLines
                        placeholderText: !Session.connection.connected ? (Session.connection.networkOnline ? i18n.tr("Connecting to Discord...")
                                                                                                          : i18n.tr("Waiting for network..."))
                                       : Session.sender.slowmodeRemaining > 0 ? i18n.tr("Slowmode: wait %1 s").arg(Session.sender.slowmodeRemaining)
                                       : i18n.tr("Message %1").arg(Session.currentChannelName)
                        readOnly: !Session.connection.connected
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.PlainText
                        onTextChanged: {
                            if (length > composer.maxMessageLength) {
                                composer.commitTyping()
                                composer.replaceText(text.substring(0, composer.maxMessageLength))
                                cursorPosition = length
                                Session.showNotice(i18n.tr("Messages cannot exceed %1 characters. Your text has been truncated to fit the limit.")
                                                   .arg(composer.maxMessageLength))
                            }
                            composer.updateMention()
                        }
                        // Also while a word is still being composed.
                        onDisplayTextChanged: {
                            if (displayText !== "" && composer.editingId === "")
                                Session.typing.notifyTyping()
                            composer.updateMention()
                        }
                        onCursorPositionChanged: composer.updateMention()
                        onActiveFocusChanged: if (activeFocus) composer.inputFocused()
                        // Enter sends on a hardware keyboard; the on-screen
                        // keyboard's Enter is a line break.
                        Keys.onReturnPressed: function(event) {
                            if (Qt.inputMethod.visible || (event.modifiers & Qt.ShiftModifier)) {
                                event.accepted = false
                                return
                            }
                            composer.send()
                            event.accepted = true
                        }
                    }
                }

                ComposerButton {
                    iconName: "send"
                    iconSize: units.gu(3)
                    enabled: Session.connection.connected
                             && (input.displayText.trim() !== "" || (composer.attachmentUrl !== "" && composer.editingId === ""))
                             && (composer.editingId !== "" || Session.sender.slowmodeRemaining === 0)
                    onClicked: composer.send()
                }
            }
        }
    }

    component ComposerButton: AbstractButton {
        property string iconName
        property real iconSize: units.gu(2.8)

        Layout.preferredWidth: units.gu(4.5)
        Layout.preferredHeight: units.gu(4.5)
        Layout.alignment: Qt.AlignBottom
        opacity: enabled ? 1 : 0.4

        Icon {
            anchors.centerIn: parent
            width: parent.iconSize
            height: width
            name: parent.iconName
            color: theme.palette.normal.backgroundText
        }
    }
}
