import QtQuick
import Lomiri.Components
import Lomiri.Components.Popups as Popups
import Disports.Core

Page {
    id: settingsPage

    header: PageHeader {
        title: i18n.tr("Settings")
    }

    Flickable {
        anchors {
            top: settingsPage.header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        contentHeight: settingsColumn.height
        clip: true

        Column {
            id: settingsColumn
            width: parent.width

            ListItem {
                height: accountLayout.height + (divider.visible ? divider.height : 0)

                ListItemLayout {
                    id: accountLayout
                    title.text: Session.username
                    title.font.bold: true

                    SidebarIcon {
                        SlotsLayout.position: SlotsLayout.Leading
                        width: units.gu(5)
                        height: width
                        imageSource: Session.avatarUrl
                    }
                }
            }

            ListItem {
                height: dmHeader.height
                divider.visible: false
                ListItemLayout {
                    id: dmHeader
                    title.text: i18n.tr("Direct messages")
                    title.font.bold: true
                }
            }

            ListItem {
                height: picturesLayout.height + (divider.visible ? divider.height : 0)
                onClicked: picturesSwitch.trigger()

                ListItemLayout {
                    id: picturesLayout
                    title.text: i18n.tr("Show profile pictures")
                    summary.text: i18n.tr("Off: icons and status only, which loads fewer images")
                    summary.wrapMode: Text.WordWrap
                    summary.maximumLineCount: 3

                    Switch {
                        id: picturesSwitch
                        SlotsLayout.position: SlotsLayout.Trailing
                        checked: Session.preferences.dmProfilePictures
                        onTriggered: Session.preferences.dmProfilePictures = checked
                    }
                }
            }

            ListItem {
                height: chatHeader.height
                divider.visible: false
                ListItemLayout {
                    id: chatHeader
                    title.text: i18n.tr("Chat")
                    title.font.bold: true
                }
            }

            ListItem {
                height: gifLayout.height + (divider.visible ? divider.height : 0)
                onClicked: gifSwitch.trigger()

                ListItemLayout {
                    id: gifLayout
                    title.text: i18n.tr("Play GIFs in the chat")
                    summary.text: i18n.tr("Off: GIFs play when you open them, which saves data and battery")
                    summary.wrapMode: Text.WordWrap
                    summary.maximumLineCount: 3

                    Switch {
                        id: gifSwitch
                        SlotsLayout.position: SlotsLayout.Trailing
                        checked: Session.preferences.autoplayGifs
                        onTriggered: Session.preferences.autoplayGifs = checked
                    }
                }
            }

            Item {
                width: parent.width
                height: units.gu(2)
            }

            Button {
                anchors { left: parent.left; right: parent.right; margins: units.gu(2) }
                text: i18n.tr("Log out")
                color: theme.palette.normal.negative
                onClicked: Popups.PopupUtils.open(logoutDialog)
            }

            Label {
                anchors { left: parent.left; right: parent.right; margins: units.gu(2) }
                topPadding: units.gu(2)
                bottomPadding: units.gu(2)
                text: i18n.tr("Disports %1 — built on Discord Messenger's client core.").arg(Qt.application.version)
                wrapMode: Text.WordWrap
                textSize: Label.Small
                color: theme.palette.normal.backgroundSecondaryText
            }
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
