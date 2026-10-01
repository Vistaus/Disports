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
    // The two-factor method picked: "totp", "sms" or "backup".
    property string method: login.methods.length > 0 ? login.methods[0] : ""

    header: PageHeader {
        title: i18n.tr("Sign in with a password")
    }

    Component.onDestruction: login.reset()

    function methodName(method) {
        return method === "totp" ? i18n.tr("Authenticator app")
             : method === "sms" ? i18n.tr("Text message (SMS)")
             : i18n.tr("Backup code")
    }

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

            // Two-factor authentication
            Label {
                width: parent.width
                visible: passwordPage.mfa
                text: i18n.tr("This account has two-factor authentication. Enter a code to finish signing in.")
                wrapMode: Text.WordWrap
            }

            OptionSelector {
                width: parent.width
                visible: passwordPage.mfa && passwordPage.login.methods.length > 1
                model: passwordPage.login.methods.map(passwordPage.methodName)
                selectedIndex: Math.max(0, passwordPage.login.methods.indexOf(passwordPage.method))
                onSelectedIndexChanged: passwordPage.method = passwordPage.login.methods[selectedIndex] || ""
            }

            Button {
                width: parent.width
                visible: passwordPage.mfa && passwordPage.method === "sms"
                text: i18n.tr("Send me a code")
                enabled: !passwordPage.login.busy
                onClicked: passwordPage.login.sendSmsCode()
            }

            TextField {
                id: codeField
                width: parent.width
                visible: passwordPage.mfa
                placeholderText: passwordPage.method === "backup" ? i18n.tr("Backup code") : i18n.tr("6-digit code")
                inputMethodHints: passwordPage.method === "backup" ? Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                                                                   : Qt.ImhDigitsOnly
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
                onClicked: passwordPage.login.verify(passwordPage.method, codeField.text)
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
