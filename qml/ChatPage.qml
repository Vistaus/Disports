import QtQuick
import Lomiri.Components
import Disports.Core

Page {
    id: chatPage
    objectName: "chatPage"

    header: PageHeader {
        title: Session.currentChannelName
        subtitle: Session.currentChannelTopic
    }

    ConnectionBanner {
        id: banner
        anchors { top: chatPage.header.bottom; left: parent.left; right: parent.right }
    }

    ChatPanel {
        anchors {
            top: banner.bottom
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
