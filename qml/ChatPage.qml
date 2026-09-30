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
        // Lomiri's own button (IconButtonStyle's sizes), with the call icon
        // green during a call.
        trailingActionBar.delegate: AbstractButton {
            id: headerButton
            action: modelData
            visible: action.visible
            implicitWidth: units.gu(4)
            height: parent ? parent.height : units.gu(4)

            Rectangle {
                anchors.fill: parent
                visible: headerButton.pressed
                color: theme.palette.highlighted.background
            }

            Icon {
                anchors.centerIn: parent
                width: units.gu(2)
                height: width
                name: headerButton.action.iconName
                color: headerButton.action.objectName === "callAction" && Session.currentChannelHasCall
                       ? theme.palette.normal.positive : theme.palette.normal.backgroundText
            }
        }
        trailingActionBar.actions: [
            Action {
                objectName: "callAction"
                // A call going on here: the active call icon, in green.
                iconName: Session.currentChannelHasCall ? "active-call" : "call-start"
                text: Session.currentChannelHasCall ? i18n.tr("Join call") : i18n.tr("Call")
                visible: Session.connection.connected && Session.currentChannelId !== ""
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
        pageStack: chatPage.pageStack
        showHeader: false
    }
}
