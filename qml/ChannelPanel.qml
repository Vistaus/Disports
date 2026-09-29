import QtQuick
import Lomiri.Components
import Disports.Core

// Channel list of the selected server, or the list of direct messages.
Item {
    id: panel

    signal channelOpened(string channelId)

    PanelHeader {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        title: Session.currentGuildName
    }

    ListView {
        id: list
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        clip: true
        model: Session.channels

        delegate: Item {
            id: row

            required property string channelId
            required property string name
            required property string kind
            required property bool isCategory
            required property bool openable
            required property bool unread
            required property int mentions
            required property string iconUrl
            required property bool indented

            readonly property bool conversation: kind === "dm" || kind === "group"
            readonly property bool active: Session.currentChannelId === channelId

            width: list.width
            height: isCategory ? units.gu(4) : (conversation ? units.gu(7) : units.gu(5.5))

            Label {
                visible: row.isCategory
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    leftMargin: units.gu(2)
                    rightMargin: units.gu(2)
                    bottomMargin: units.gu(0.5)
                }
                text: row.name.toUpperCase()
                font.pixelSize: units.gu(1.3)
                font.bold: true
                elide: Text.ElideRight
                color: theme.palette.normal.backgroundSecondaryText
            }

            ListItem {
                anchors.fill: parent
                visible: !row.isCategory
                enabled: row.openable
                opacity: row.openable ? 1 : 0.6
                divider.visible: false
                color: row.active ? theme.palette.highlighted.background : "transparent"
                onClicked: panel.channelOpened(row.channelId)

                Row {
                    anchors {
                        left: parent.left
                        right: parent.right
                        leftMargin: units.gu(row.indented ? 3 : 2)
                        rightMargin: units.gu(2)
                        verticalCenter: parent.verticalCenter
                    }
                    spacing: units.gu(row.conversation ? 1.5 : 1)

                    SidebarIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.conversation
                        width: units.gu(4.5)
                        height: width
                        imageSource: row.iconUrl
                        iconName: row.iconUrl === "" ? "contact-group" : ""
                    }

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        width: units.gu(2)
                        height: width
                        visible: row.kind === "voice" || row.kind === "announcement" || row.kind === "forum"
                        name: row.kind === "voice" ? "audio-speakers-symbolic"
                                                   : (row.kind === "announcement" ? "notification" : "message")
                        color: theme.palette.normal.backgroundSecondaryText
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.kind === "text"
                        text: "#"
                        font.pixelSize: units.gu(1.8)
                        font.bold: true
                        color: theme.palette.normal.backgroundSecondaryText
                    }

                    Label {
                        readonly property bool emphasized: row.unread || row.mentions > 0 || row.conversation

                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - (row.conversation ? units.gu(10) : units.gu(7))
                        text: row.name
                        font.pixelSize: units.gu(row.conversation ? 1.8 : 1.7)
                        font.bold: row.unread || row.mentions > 0
                        color: emphasized ? theme.palette.normal.backgroundText
                                          : theme.palette.normal.backgroundSecondaryText
                        elide: Text.ElideRight
                    }

                    UnreadBadge {
                        anchors.verticalCenter: parent.verticalCenter
                        mentions: row.mentions
                    }
                }
            }
        }

        Label {
            anchors.centerIn: parent
            visible: list.count === 0
            text: Session.inDirectMessages ? i18n.tr("No conversations yet") : i18n.tr("No channels")
            color: theme.palette.normal.backgroundSecondaryText
        }
    }
}
