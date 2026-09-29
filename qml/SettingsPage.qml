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
