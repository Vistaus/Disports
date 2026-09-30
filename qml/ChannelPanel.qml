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
            required property string status
            required property bool blocked

            readonly property bool conversation: kind === "dm" || kind === "group"
            readonly property bool pictures: Session.preferences.dmProfilePictures
            readonly property bool active: Session.currentChannelId === channelId

            width: list.width
            height: isCategory ? units.gu(4) : (conversation && pictures ? units.gu(7) : units.gu(5.5))

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
                // Separator lines between contacts, groups and channels
                divider.visible: true
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
                    spacing: units.gu(row.conversation ? 1.25 : 1)

                    // Direct messages with profile pictures
                    Item {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.conversation && row.pictures
                        width: units.gu(4.5)
                        height: width

                        SidebarIcon {
                            anchors.fill: parent
                            imageSource: row.iconUrl
                            iconName: row.kind === "group" && row.iconUrl === "" ? "contact-group" : ""
                            label: row.name.charAt(0)
                        }

                        Rectangle {
                            visible: row.kind === "dm"
                            anchors { right: parent.right; bottom: parent.bottom; margins: -units.dp(2) }
                            width: units.gu(1.6)
                            height: width
                            radius: width / 2
                            color: theme.palette.normal.background

                            StatusDot {
                                anchors.centerIn: parent
                                visible: !row.blocked
                                status: row.status
                            }

                            // Blocked, as in the Qt 5 version
                            Icon {
                                anchors.centerIn: parent
                                visible: row.blocked
                                width: units.gu(1.3)
                                height: width
                                name: "cancel"
                                color: theme.palette.normal.negative
                            }
                        }
                    }

                    // Direct messages with icons only
                    StatusDot {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.kind === "dm" && !row.pictures && !row.blocked
                        status: row.status
                    }

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.kind === "dm" && !row.pictures && row.blocked
                        width: units.gu(1.5)
                        height: width
                        name: "cancel"
                        color: theme.palette.normal.negative
                    }

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        width: units.gu(2)
                        height: width
                        visible: row.kind === "group" && !row.pictures
                        name: "contact-group"
                        color: theme.palette.normal.backgroundSecondaryText
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
                        width: parent.width - (row.conversation && row.pictures ? units.gu(10) : units.gu(7))
                        text: row.name
                        font.pixelSize: units.gu(row.conversation && row.pictures ? 1.8 : 1.7)
                        font.bold: row.unread || row.mentions > 0
                        font.strikeout: row.blocked
                        color: emphasized && !row.blocked ? theme.palette.normal.backgroundText
                                                          : theme.palette.normal.backgroundSecondaryText
                        elide: Text.ElideRight
                    }

                    UnreadBadge {
                        anchors.verticalCenter: parent.verticalCenter
                        mentions: row.mentions
                        unread: row.unread && !row.conversation
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
