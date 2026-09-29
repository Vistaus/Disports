import QtQuick
import Lomiri.Components
import Disports.Core

// The server rail: Direct Messages first, then servers.
Item {
    id: sidebar

    signal directMessagesSelected()
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
        clip: true
        model: Session.guilds
        spacing: units.gu(1)

        header: Column {
            width: rail.width
            spacing: units.gu(1)
            bottomPadding: units.gu(1)

            Item {
                width: parent.width
                height: units.gu(5)

                SelectionMarker {
                    selected: Session.inDirectMessages
                }

                SidebarIcon {
                    anchors.centerIn: parent
                    iconName: "message"
                    highlighted: Session.inDirectMessages
                }

                UnreadBadge {
                    anchors { right: parent.right; bottom: parent.bottom; rightMargin: units.gu(0.5) }
                    mentions: Session.guilds.directMessageMentions
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: sidebar.directMessagesSelected()
                }
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: units.gu(4)
                height: units.dp(2)
                color: theme.palette.normal.base
            }
        }

        delegate: Item {
            id: row
            required property string guildId
            required property string name
            required property string iconUrl
            required property string initials
            required property bool unread
            required property int mentions

            readonly property bool selected: !Session.inDirectMessages && Session.currentGuildId === guildId

            width: rail.width
            height: units.gu(5)

            SelectionMarker {
                selected: row.selected
                unread: row.unread
            }

            SidebarIcon {
                anchors.centerIn: parent
                imageSource: row.iconUrl
                label: row.initials
                highlighted: row.selected
            }

            UnreadBadge {
                anchors { right: parent.right; bottom: parent.bottom; rightMargin: units.gu(0.5) }
                mentions: row.mentions
            }

            MouseArea {
                anchors.fill: parent
                onClicked: sidebar.guildSelected(row.guildId)
            }
        }
    }

    // The pill on the left edge: tall when selected, a dot when unread.
    component SelectionMarker: Rectangle {
        property bool selected: false
        property bool unread: false

        anchors { left: parent.left; verticalCenter: parent.verticalCenter }
        width: units.gu(0.5)
        height: selected ? units.gu(4) : units.gu(1)
        radius: width / 2
        visible: selected || unread
        color: theme.palette.normal.backgroundText

        Behavior on height {
            NumberAnimation { duration: 150 }
        }
    }
}
