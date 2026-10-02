import QtQuick
import Lomiri.Components
import Disports.Core

// Someone's profile: picture, name, username and pronouns, status, about
// me, ID, and a way to their DM. `user` is Session.userInfo() (see
// describeUser() in src/ChannelInfo.h). Used by ProfilePage and, for a 1:1
// DM, ChannelInfoPage.
Column {
    id: view

    property var user: ({})
    // Shows "Message" when there is a DM with them to go to (not on the
    // DM's own info page).
    property string directMessageId: ""

    signal messageRequested(string channelId)

    function statusText(status) {
        return status === "online" ? i18n.tr("Online")
             : status === "idle" ? i18n.tr("Idle")
             : status === "dnd" ? i18n.tr("Do not disturb")
             : i18n.tr("Offline")
    }

    ListItem {
        height: personColumn.height + units.gu(4) + divider.height

        Column {
            id: personColumn
            anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: units.gu(2) }
            spacing: units.gu(1)

            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: units.gu(12)
                height: width

                RoundedPicture {
                    anchors.fill: parent
                    source: view.user.avatarUrl || ""
                    label: (view.user.name || "").charAt(0)
                }

                Rectangle {
                    anchors { right: parent.right; bottom: parent.bottom; margins: -units.dp(2) }
                    width: units.gu(3)
                    height: width
                    radius: width / 2
                    color: theme.palette.normal.background

                    StatusDot {
                        anchors.centerIn: parent
                        width: units.gu(2.2)
                        height: width
                        status: view.user.status || "offline"
                    }
                }
            }

            Label {
                width: parent.width - units.gu(4)
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                text: view.user.name || ""
                textSize: Label.Large
                font.bold: true
                wrapMode: Text.Wrap
            }

            Label {
                width: parent.width - units.gu(4)
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                text: ["@" + (view.user.username || ""), view.user.pronouns || "",
                       view.user.bot ? i18n.tr("Bot") : ""].filter(part => part !== "").join(" · ")
                color: theme.palette.normal.backgroundSecondaryText
                wrapMode: Text.Wrap
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: view.directMessageId !== "" && !view.user.self
                text: i18n.tr("Message")
                color: theme.palette.normal.positive
                onClicked: view.messageRequested(view.directMessageId)
            }
        }
    }

    ListItem {
        height: statusLayout.height + divider.height

        ListItemLayout {
            id: statusLayout
            title.text: view.statusText(view.user.status)
            subtitle.text: view.user.customStatus || ""
            subtitle.wrapMode: Text.Wrap
            subtitle.maximumLineCount: 3

            StatusDot {
                SlotsLayout.position: SlotsLayout.Leading
                width: units.gu(1.6)
                height: width
                status: view.user.status || "offline"
            }
        }
    }

    ListItem {
        visible: (view.user.bio || "") !== ""
        height: visible ? bioLayout.height + divider.height : 0

        ListItemLayout {
            id: bioLayout
            title.text: i18n.tr("About me")
            title.font.bold: true
            summary.text: view.user.bio || ""
            summary.wrapMode: Text.Wrap
            summary.maximumLineCount: 30
        }
    }

    ListItem {
        visible: !!view.user.blocked
        height: visible ? blockedLayout.height + divider.height : 0

        ListItemLayout {
            id: blockedLayout
            title.text: i18n.tr("You blocked this user")
            title.color: theme.palette.normal.negative

            Icon {
                SlotsLayout.position: SlotsLayout.Leading
                width: units.gu(2.5)
                height: width
                name: "cancel"
                color: theme.palette.normal.negative
            }
        }
    }

    ListItem {
        height: idLayout.height + divider.height

        trailingActions: ListItemActions {
            actions: [
                Action {
                    iconName: "edit-copy"
                    text: i18n.tr("Copy")
                    onTriggered: Clipboard.push(view.user.id || "")
                }
            ]
        }

        ListItemLayout {
            id: idLayout
            title.text: i18n.tr("ID")
            subtitle.text: view.user.id || ""
        }
    }
}
