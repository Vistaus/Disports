import QtQuick
import Lomiri.Components
import Lomiri.Components.Popups as Popups
import Disports.Core

Page {
    id: settingsPage

    header: PageHeader {
        title: i18n.tr("Settings")
    }

    Column {
        anchors {
            top: settingsPage.header.bottom
            left: parent.left
            right: parent.right
            margins: units.gu(2)
        }
        spacing: units.gu(2)

        Row {
            spacing: units.gu(2)

            SidebarIcon {
                width: units.gu(6)
                height: width
                imageSource: Session.avatarUrl
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: Session.username
                font.pixelSize: units.gu(2)
                font.bold: true
            }
        }

        Rectangle {
            width: parent.width
            height: units.dp(1)
            color: theme.palette.normal.base
        }

        Label {
            text: i18n.tr("Direct messages")
            font.pixelSize: units.gu(1.6)
            font.bold: true
        }

        Item {
            width: parent.width
            height: units.gu(4.5)

            Column {
                anchors {
                    left: parent.left
                    right: picturesSwitch.left
                    rightMargin: units.gu(2)
                    verticalCenter: parent.verticalCenter
                }
                spacing: units.gu(0.5)

                Label {
                    width: parent.width
                    text: i18n.tr("Show profile pictures")
                }

                Label {
                    width: parent.width
                    text: i18n.tr("Off: icons and status only, which loads fewer images")
                    font.pixelSize: units.gu(1.4)
                    color: theme.palette.normal.backgroundSecondaryText
                    wrapMode: Text.WordWrap
                }
            }

            Switch {
                id: picturesSwitch
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                checked: Session.preferences.dmProfilePictures
                onClicked: Session.preferences.dmProfilePictures = checked
            }
        }

        Rectangle {
            width: parent.width
            height: units.dp(1)
            color: theme.palette.normal.base
        }

        Button {
            width: parent.width
            text: i18n.tr("Log out")
            color: theme.palette.normal.negative
            onClicked: Popups.PopupUtils.open(logoutDialog)
        }

        Label {
            width: parent.width
            text: i18n.tr("Disports %1 — built on Discord Messenger's client core.").arg(Qt.application.version)
            wrapMode: Text.WordWrap
            font.pixelSize: units.gu(1.3)
            color: theme.palette.normal.backgroundSecondaryText
        }
    }

    Component {
        id: logoutDialog

        Popups.Dialog {
            id: dialog
            title: i18n.tr("Log out?")
            text: i18n.tr("You'll need to sign in again to use Disports.")

            Button {
                text: i18n.tr("Cancel")
                onClicked: Popups.PopupUtils.close(dialog)
            }

            Button {
                text: i18n.tr("Log out")
                color: theme.palette.normal.negative
                onClicked: {
                    Popups.PopupUtils.close(dialog)
                    Session.logout()
                }
            }
        }
    }
}
