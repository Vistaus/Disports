import QtQuick
import Lomiri.Components
import Disports.Core

// The server rail: Direct Messages, conversations with unread messages,
// then servers and server folders.
Item {
    id: sidebar

    signal directMessagesSelected()
    signal directMessageOpened(string channelId)
    signal guildSelected(string guildId)

    width: units.gu(7)

    Rectangle {
        anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
        width: units.dp(1)
        color: theme.palette.normal.base
    }

    ListView {
        id: rail
        anchors.fill: parent
        anchors.topMargin: units.gu(1)
        anchors.bottomMargin: units.gu(0.5)
        clip: true
        model: Session.guilds
        cacheBuffer: units.gu(80)

        header: Column {
            width: rail.width
            spacing: units.gu(0.5)

            // Direct Messages
            Item {
                width: parent.width
                height: sidebar.width

                Rectangle {
                    anchors.fill: parent
                    color: dmMouse.pressed || Session.inDirectMessages
                           ? theme.palette.highlighted.base
                           : "transparent"
                }

                SidebarIcon {
                    anchors.centerIn: parent
                    iconName: "contact"
                    label: i18n.tr("DMs")
                    showTileBackground: true
                }

                MouseArea {
                    id: dmMouse
                    anchors.fill: parent
                    onClicked: sidebar.directMessagesSelected()
                }
            }

            // Conversations with unread messages
            Repeater {
                model: Session.unreadDirectMessages

                delegate: Item {
                    id: dmRow

                    required property string channelId
                    required property string name
                    required property string iconUrl
                    required property string initials
                    required property int mentions

                    width: rail.width
                    height: sidebar.width

                    Rectangle {
                        anchors.fill: parent
                        color: dmRowMouse.pressed ? theme.palette.highlighted.base : "transparent"
                    }

                    SidebarIcon {
                        anchors.centerIn: parent
                        imageSource: dmRow.iconUrl
                        label: dmRow.initials
                    }

                    UnreadBadge {
                        anchors {
                            bottom: parent.bottom
                            right: parent.right
                            bottomMargin: units.gu(0.5)
                            rightMargin: units.gu(0.5)
                        }
                        mentions: dmRow.mentions
                    }

                    MouseArea {
                        id: dmRowMouse
                        anchors.fill: parent
                        onClicked: sidebar.directMessageOpened(dmRow.channelId)
                    }
                }
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: units.gu(4)
                height: units.dp(1)
                color: theme.palette.normal.base
            }

            Item {
                width: parent.width
                height: units.gu(0.5)
            }
        }

        delegate: Item {
            id: row

            required property string kind
            required property string itemId
            required property string name
            required property string iconUrl
            required property string initials
            required property bool unread
            required property int mentions
            required property string folderId
            required property string folderColor
            required property bool expanded
            required property var previews
            required property var guildIds
            required property bool hasCall

            readonly property bool isFolder: kind === "folder"
            readonly property bool inFolder: !isFolder && folderId !== ""
            readonly property bool selected: !Session.inDirectMessages
                                             && (isFolder ? (!expanded && guildIds.indexOf(Session.currentGuildId) >= 0)
                                                          : Session.currentGuildId === itemId)

            width: rail.width
            height: isFolder && expanded ? units.gu(2.2) : sidebar.width

            // Folder colour stripe on the folder and on its servers.
            Rectangle {
                id: stripe
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                width: row.isFolder || row.inFolder ? units.dp(3) : 0
                visible: width > 0
                color: row.folderColor !== "" ? row.folderColor : theme.palette.normal.base
                opacity: row.folderColor !== "" ? 1 : 0.4
            }

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: stripe.width
                visible: !(row.isFolder && row.expanded) && (rowMouse.pressed || row.selected)
                color: theme.palette.highlighted.base
            }

            // Server
            SidebarIcon {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: stripe.width / 2
                visible: !row.isFolder
                imageSource: row.iconUrl
                label: row.initials
            }

            // Collapsed folder
            FolderPreviewIcon {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: stripe.width / 2
                visible: row.isFolder && !row.expanded
                width: units.gu(5)
                height: width
                previews: row.previews
            }

            // Expanded folder: a small header with its name
            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: stripe.width
                visible: row.isFolder && row.expanded
                color: theme.palette.normal.base
                opacity: 0.35
            }

            Label {
                anchors {
                    left: parent.left
                    right: chevron.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: stripe.width + units.gu(0.25)
                    rightMargin: units.gu(0.25)
                }
                visible: row.isFolder && row.expanded
                text: row.name
                font.pixelSize: units.gu(1.05)
                font.bold: true
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                color: theme.palette.normal.backgroundSecondaryText
            }

            Icon {
                id: chevron
                anchors { right: parent.right; rightMargin: units.gu(0.25); verticalCenter: parent.verticalCenter }
                visible: row.isFolder && row.expanded
                width: units.gu(1.6)
                height: width
                name: "go-up"
                color: theme.palette.normal.backgroundSecondaryText
            }

            // Someone is in a voice channel there.
            CallBadge {
                anchors {
                    top: parent.top
                    right: parent.right
                    topMargin: units.gu(0.3)
                    rightMargin: units.gu(0.3)
                }
                visible: row.hasCall && !(row.isFolder && row.expanded)
            }

            UnreadBadge {
                anchors {
                    bottom: parent.bottom
                    right: parent.right
                    bottomMargin: units.gu(0.5)
                    rightMargin: units.gu(0.5)
                }
                visible: (row.mentions > 0 || row.unread) && !(row.isFolder && row.expanded)
                mentions: row.mentions
                unread: row.unread
            }

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                onClicked: {
                    if (row.isFolder)
                        Session.guilds.toggleFolder(row.itemId)
                    else
                        sidebar.guildSelected(row.itemId)
                }
            }
        }

        // Fade at the bottom while there is more to scroll to.
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: units.gu(3)
            visible: rail.contentHeight > rail.height
                     && rail.visibleArea.yPosition + rail.visibleArea.heightRatio < 0.995
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: theme.palette.normal.background }
            }
        }
    }
}
