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
        // As Lomiri's own, with the call icon green during a call.
        trailingActionBar.delegate: AbstractButton {
            id: headerButton
            action: modelData
            visible: action.visible
            width: units.gu(4.5)
            height: units.gu(6)

            Icon {
                anchors.centerIn: parent
                width: units.gu(2.5)
                height: width
                name: headerButton.action.iconName
                color: headerButton.action.objectName === "callAction" && Session.currentChannelHasCall
                       ? theme.palette.normal.positive : theme.palette.normal.backgroundText
                opacity: headerButton.pressed ? 0.6 : 1
            }
        }
        trailingActionBar.actions: [
            Action {
                objectName: "callAction"
                // A call going on here: the active call icon, in green.
                iconName: Session.currentChannelHasCall ? "active-call" : "call-start"
                text: Session.currentChannelHasCall ? i18n.tr("Join call") : i18n.tr("Call")
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
        id: panel
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
        onAttachRequested: {
            const picker = chatPage.pageStack.push(Qt.resolvedUrl("ContentPickerPage.qml"))
            picker.picked.connect(function(url, transfer) { panel.attach(url, transfer) })
        }
    }
}
