import QtQuick
import Lomiri.Components
import Disports.Core

Page {
    id: mainPage
    objectName: "mainPage"

    property var stack
    property var root
    readonly property bool wideLayout: root ? root.wideLayout : false

    function openChannel(channelId) {
        Session.openChannel(channelId)
        if (!wideLayout)
            stack.push(Qt.resolvedUrl("ChatPage.qml"))
    }

    header: PageHeader {
        title: i18n.tr("Disports")
        trailingActionBar.actions: [
            Action {
                iconName: "settings"
                text: i18n.tr("Settings")
                onTriggered: mainPage.stack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
        ]
    }

    ConnectionBanner {
        id: banner
        anchors { top: mainPage.header.bottom; left: parent.left; right: parent.right }
    }

    Row {
        anchors {
            top: banner.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        Sidebar {
            id: sidebar
            height: parent.height
            onDirectMessagesSelected: Session.selectDirectMessages()
            onGuildSelected: function(guildId) { Session.selectGuild(guildId) }
        }

        ChannelPanel {
            width: mainPage.wideLayout ? units.gu(32) : parent.width - sidebar.width
            height: parent.height
            onChannelOpened: function(channelId) { mainPage.openChannel(channelId) }
        }

        Loader {
            width: mainPage.wideLayout ? parent.width - sidebar.width - units.gu(32) : 0
            height: parent.height
            active: mainPage.wideLayout
            visible: active

            sourceComponent: Item {
                Rectangle {
                    anchors { top: parent.top; bottom: parent.bottom; left: parent.left }
                    width: units.dp(1)
                    color: theme.palette.normal.base
                }

                ChatPanel {
                    anchors.fill: parent
                    anchors.leftMargin: units.dp(1)
                    visible: Session.currentChannelId !== ""
                }

                Label {
                    anchors.centerIn: parent
                    visible: Session.currentChannelId === ""
                    text: i18n.tr("Pick a conversation")
                    color: theme.palette.normal.backgroundSecondaryText
                }
            }
        }
    }
}
