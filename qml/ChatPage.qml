import QtQuick
import Lomiri.Components
import Disports.Core

Page {
    id: chatPage
    objectName: "chatPage"

    header: PageHeader {
        title: Session.currentChannelName
        // Channel topics can have several lines; the header has room for one.
        subtitle: Session.currentChannelTopic.replace(/\s*\n+\s*/g, "  ")
        trailingActionBar.actions: [
            Action {
                iconName: "call-start"
                text: i18n.tr("Call")
                visible: Session.connected && Session.currentChannelId !== ""
                         && Session.call.canCall(Session.currentChannelId)
                onTriggered: Session.call.start(Session.currentChannelId)
            },
            Action {
                iconName: "info"
                text: i18n.tr("Info")
                onTriggered: chatPage.pageStack.push(Qt.resolvedUrl("ChannelInfoPage.qml"),
                                                     { "channelId": Session.currentChannelId })
            }
        ]
    }

    ChatPanel {
        anchors {
            top: chatPage.header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        showHeader: false
        onMediaOpened: function(media) {
            if (media.viewType === "none")
                Qt.openUrlExternally(media.openUrl || media.viewUrl)
            else
                chatPage.pageStack.push(Qt.resolvedUrl("MediaViewerPage.qml"), { "media": media })
        }
    }
}
