import QtQuick
import Lomiri.Components
import Lomiri.Components.Popups as Popups
import Disports.Core

Page {
    id: loginPage

    header: PageHeader {
        title: i18n.tr("Sign in")
    }

    Flickable {
        anchors {
            top: loginPage.header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: units.gu(2)
        }
        contentHeight: content.height
        clip: true

        Column {
            id: content
            width: parent.width
            spacing: units.gu(2)

            Label {
                width: parent.width
                text: i18n.tr("Scan the QR code with the Discord mobile app.")
                wrapMode: Text.WordWrap
                color: theme.palette.normal.backgroundSecondaryText
            }

            Rectangle {
                width: Math.min(parent.width, units.gu(28))
                height: width
                anchors.horizontalCenter: parent.horizontalCenter
                radius: units.gu(0.75)
                color: theme.palette.normal.base

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: units.gu(1)
                    radius: units.gu(0.5)
                    color: "white"

                    Image {
                        anchors.fill: parent
                        anchors.margins: units.gu(1)
                        source: Session.qrLogin.qrImage
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        cache: false
                    }

                    ActivityIndicator {
                        anchors.centerIn: parent
                        running: Session.qrLogin.qrImage === "" && Session.qrLogin.busy
                    }
                }
            }

            Label {
                width: parent.width
                text: Session.qrLogin.status
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: Session.qrLogin.failed ? theme.palette.normal.negative
                                              : theme.palette.normal.backgroundSecondaryText
            }

            Button {
                width: parent.width
                text: i18n.tr("Refresh QR code")
                enabled: !Session.qrLogin.busy
                onClicked: Session.qrLogin.start()
            }

            Label {
                width: parent.width
                visible: Session.errorText !== ""
                text: Session.errorText
                wrapMode: Text.WordWrap
                color: theme.palette.normal.negative
            }

            Rectangle {
                width: parent.width
                height: units.dp(1)
                color: theme.palette.normal.base
            }

            Button {
                width: parent.width
                text: i18n.tr("Sign in with a password (not recommended)")
                onClicked: loginPage.pageStack.push(Qt.resolvedUrl("PasswordLoginPage.qml"))
            }

            Button {
                width: parent.width
                text: i18n.tr("Paste a token instead (not recommended)")
                onClicked: Popups.PopupUtils.open(tokenDialog)
            }

            Label {
                width: parent.width
                text: i18n.tr("Using any unofficial Discord client is against Discord's Terms of Service and may result in your account being restricted or banned.")
                wrapMode: Text.WordWrap
                font.pixelSize: units.gu(1.4)
                color: theme.palette.normal.backgroundSecondaryText
            }
        }
    }

    Component {
        id: tokenDialog

        Popups.Dialog {
            id: dialog
            title: i18n.tr("Sign in with a token")
            text: i18n.tr("Paste your Discord account token to sign in directly.")

            TextField {
                id: tokenField
                placeholderText: i18n.tr("Token")
                echoMode: TextInput.Password
            }

            Button {
                text: i18n.tr("Cancel")
                onClicked: Popups.PopupUtils.close(dialog)
            }

            Button {
                text: i18n.tr("Sign in")
                color: theme.palette.normal.positive
                enabled: tokenField.text.trim() !== ""
                onClicked: {
                    const token = tokenField.text
                    Popups.PopupUtils.close(dialog)
                    Session.loginWithToken(token)
                }
            }
        }
    }
}
