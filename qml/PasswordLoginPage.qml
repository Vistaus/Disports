import QtQuick
import Lomiri.Components
import Disports.Core

// Signing in with email (or phone number) and password, then the
// two-factor code if the account has one. See src/PasswordLogin.h.
Page {
    id: passwordPage
    objectName: "passwordLoginPage"

    readonly property var login: Session.passwordLogin
    readonly property bool mfa: login.step === "mfa"
    readonly property bool hasApp: login.methods.indexOf("totp") >= 0
    readonly property bool hasSms: login.methods.indexOf("sms") >= 0
    readonly property bool hasBackup: login.methods.indexOf("backup") >= 0

    header: PageHeader {
        title: i18n.tr("Sign in with a password")
    }

    Component.onDestruction: login.reset()

    Flickable {
        anchors {
            top: passwordPage.header.bottom
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
                visible: !passwordPage.mfa
                text: i18n.tr("Signing in with a password from an unofficial app makes Discord more suspicious of the account. The QR code is the safer way.")
                wrapMode: Text.WordWrap
                color: theme.palette.normal.backgroundSecondaryText
            }

            TextField {
                id: loginField
                width: parent.width
                visible: !passwordPage.mfa
                placeholderText: i18n.tr("Email or phone number")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhEmailCharactersOnly
                enabled: !passwordPage.login.busy
            }

            TextField {
                id: passwordField
                width: parent.width
                visible: !passwordPage.mfa
                placeholderText: i18n.tr("Password")
                echoMode: TextInput.Password
                enabled: !passwordPage.login.busy
                onAccepted: signInButton.clicked()
            }

            Button {
                id: signInButton
                width: parent.width
                visible: !passwordPage.mfa
                text: i18n.tr("Sign in")
                color: theme.palette.normal.positive
                enabled: !passwordPage.login.busy && loginField.text.trim() !== "" && passwordField.text !== ""
                onClicked: passwordPage.login.login(loginField.text, passwordField.text)
            }

            // Two-factor authentication: one field; the kind of code is told
            // by its shape (see PasswordLogin::verify).
            Label {
                width: parent.width
                visible: passwordPage.mfa
                text: ((passwordPage.login.smsSent ? i18n.tr("Enter the code from the text message.")
                       : passwordPage.hasApp ? i18n.tr("Enter the code from your authenticator app.")
                       : "")
                      + (passwordPage.hasBackup ? " " + i18n.tr("You can also use a backup code.") : "")).trim()
                wrapMode: Text.WordWrap
            }

            TextField {
                id: codeField
                width: parent.width
                visible: passwordPage.mfa
                placeholderText: i18n.tr("Code")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                enabled: !passwordPage.login.busy
                onAccepted: verifyButton.clicked()
            }

            Button {
                id: verifyButton
                width: parent.width
                visible: passwordPage.mfa
                text: i18n.tr("Verify")
                color: theme.palette.normal.positive
                enabled: !passwordPage.login.busy && codeField.text.trim() !== ""
                onClicked: passwordPage.login.verify(codeField.text)
            }

            Button {
                width: parent.width
                visible: passwordPage.mfa && passwordPage.hasSms && passwordPage.hasApp && !passwordPage.login.smsSent
                text: i18n.tr("Text me a code instead")
                enabled: !passwordPage.login.busy
                onClicked: passwordPage.login.sendSmsCode()
            }

            Button {
                width: parent.width
                visible: passwordPage.mfa
                text: i18n.tr("Start again")
                enabled: !passwordPage.login.busy
                onClicked: {
                    codeField.text = ""
                    passwordPage.login.reset()
                }
            }

            ActivityIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: passwordPage.login.busy
                visible: running
            }

            Label {
                width: parent.width
                visible: text !== ""
                text: passwordPage.login.notice
                wrapMode: Text.WordWrap
                color: theme.palette.normal.backgroundSecondaryText
            }

            Label {
                width: parent.width
                visible: text !== ""
                text: passwordPage.login.error
                wrapMode: Text.WordWrap
                color: theme.palette.normal.negative
            }
        }
    }
}
