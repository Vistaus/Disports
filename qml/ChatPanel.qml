import QtQuick
import QtQuick.Layouts
import Lomiri.Components
import Disports.Core

// Messages of the open channel plus the composer.
Item {
    id: chatPanel

    property bool showHeader: true
    property string replyToId: ""
    property string replyToAuthor: ""

    function send() {
        const text = input.text
        if (text.trim() === "")
            return
        Session.sendMessage(text, chatPanel.replyToId)
        input.text = ""
        chatPanel.replyToId = ""
    }

    Connections {
        target: Session
        function onCurrentChannelChanged() {
            chatPanel.replyToId = ""
            input.text = ""
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

        delegate: MessageDelegate {
            width: messageList.width
            onReplyRequested: function(messageId, author) {
                chatPanel.replyToId = messageId
                chatPanel.replyToAuthor = author
                input.forceActiveFocus()
            }
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

        // Load older history automatically when scrolled to the top.
        onAtYEndChanged: {
            if (atYEnd && count > 0 && Session.messages.hasOlder && !Session.loadingMessages)
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
        height: chatPanel.replyToId !== "" ? units.gu(4) : 0
        visible: height > 0
        color: theme.palette.normal.base

        Label {
            anchors { left: parent.left; leftMargin: units.gu(2); verticalCenter: parent.verticalCenter }
            text: i18n.tr("Replying to %1").arg(chatPanel.replyToAuthor)
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
                onClicked: chatPanel.replyToId = ""
            }
        }
    }

    Rectangle {
        id: composer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
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

            TextArea {
                id: input
                Layout.fillWidth: true
                autoSize: true
                maximumLineCount: 4
                placeholderText: Session.connected ? i18n.tr("Message %1").arg(Session.currentChannelName)
                                                   : i18n.tr("Waiting for connection…")
                readOnly: !Session.connected
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                onTextChanged: if (text !== "") Session.notifyTyping()
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
                    source: "qrc:/assets/send.svg"
                    color: theme.palette.normal.backgroundText
                }
            }
        }
    }
}
