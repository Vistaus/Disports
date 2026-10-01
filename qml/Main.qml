import QtQuick
import Lomiri.Components
import Disports.Core

MainView {
    id: root

    applicationName: "disports.jukfiuu"
    // With Qt 6 on Ubuntu Touch the reported keyboard height isn't scaled
    // for HiDPI, and MainView would push the app off screen. We keep room
    // for the keyboard ourselves, like Morph does.
    anchorToKeyboard: false

    // Settings > Theme; "" follows the system.
    theme.name: Session.preferences.themeMode === 0 ? "Lomiri.Components.Themes.Ambiance"
              : Session.preferences.themeMode === 1 ? "Lomiri.Components.Themes.SuruDark"
              : ""
    width: units.gu(45)
    height: units.gu(75)

    readonly property bool wideLayout: width >= units.gu(90)

    readonly property real keyboardHeight: Qt.inputMethod.visible
        ? Math.min(Qt.inputMethod.keyboardRectangle.height / (units.gridUnit / 8), height / 2)
        : 0

    function showPhase() {
        switch (Session.phase) {
        case SessionPhase.LoggedOut:
            stack.clear()
            stack.push(Qt.resolvedUrl("LoginPage.qml"))
            Session.qrLogin.start()
            break
        case SessionPhase.Connecting:
        case SessionPhase.Ready:
            if (!stack.currentPage || stack.currentPage.objectName !== "mainPage") {
                stack.clear()
                stack.push(Qt.resolvedUrl("MainPage.qml"), { "stack": stack, "root": root })
            }
            break
        }
    }

    Component.onCompleted: showPhase()

    // Only in test builds (DISPORTS_TEST_HOOKS), see src/testing/TestHooks.h.
    Loader {
        active: typeof testHooks !== "undefined"
        source: "TestHooks.qml"
        onLoaded: {
            item.stack = stack
            item.root = root
        }
    }

    readonly property string currentPageName: stack.currentPage ? stack.currentPage.objectName : ""

    // A chat is on screen: inline next to the channel list in the wide
    // layout, or as its own page on a phone.
    Binding {
        target: Session
        property: "chatVisible"
        value: Session.currentChannelId !== ""
               && ((root.wideLayout && root.currentPageName === "mainPage")
                   || root.currentPageName === "chatPage")
    }

    Binding {
        target: Session
        property: "autoSelectChannel"
        value: root.wideLayout
    }

    // Growing into the wide layout shows the chat inline instead.
    onWideLayoutChanged: {
        if (wideLayout && currentPageName === "chatPage")
            stack.pop()
    }

    Connections {
        target: Session
        function onPhaseChanged() { root.showPhase() }
    }

    // Captchas Discord wants solved: their page comes up over everything.
    // It needs the system's Qt WebEngine (Ubuntu Touch 24.04-2.x).
    Connections {
        target: Session.captcha
        function onRequested() {
            if (root.currentPageName === "captchaPage")
                return
            // Test runs answer it themselves (DISPORTS_CAPTCHA_TOKEN).
            if (typeof testHooks !== "undefined" && testHooks.answersCaptchas)
                return
            const page = Qt.createComponent(Qt.resolvedUrl("CaptchaPage.qml"))
            if (page.status !== Component.Ready) {
                Session.captcha.cancel()
                Session.showNotice(i18n.tr("Discord wants a captcha solved, but this version of Ubuntu Touch can't show one. Update to 24.04-2.x or newer."))
                return
            }
            stack.push(page)
        }
        function onChanged() {
            if (!Session.captcha.active && root.currentPageName === "captchaPage")
                stack.pop()
        }
    }

    // Calls: the call screen comes up when one starts or rings, and goes
    // away when it ends.
    Connections {
        target: Session.call
        function onShowRequested() {
            if (root.currentPageName !== "callPage")
                stack.push(Qt.resolvedUrl("CallPage.qml"))
        }
        function onStateChanged() {
            if (Session.call.state === CallState.Idle && root.currentPageName === "callPage")
                stack.pop()
        }
    }

    // Offline / reconnecting: above everything, the page title included.
    ConnectionBanner {
        id: connectionBanner
        anchors { top: parent.top; left: parent.left; right: parent.right }
        z: 2
    }

    // Connecting, or loading a conversation.
    CallBanner {
        id: callBanner
        anchors { top: connectionBanner.bottom; left: parent.left; right: parent.right }
        z: 2
        hidden: root.currentPageName === "callPage"
    }

    ProgressBar {
        anchors { top: callBanner.bottom; left: parent.left; right: parent.right }
        z: 2
        indeterminate: true
        visible: Session.connection.networkOnline
                 && ((connectionBanner.reconnecting && Session.connection.reconnectSeconds === 0)
                     || Session.loadingMessages)
    }

    PageStack {
        id: stack
        anchors.fill: parent
        anchors.topMargin: connectionBanner.height + callBanner.height
        anchors.bottomMargin: root.keyboardHeight

        Behavior on anchors.bottomMargin {
            LomiriNumberAnimation {}
        }
    }

    // Splash while signing in for the first time.
    Rectangle {
        anchors.fill: parent
        z: 3
        color: theme.palette.normal.background
        visible: Session.phase === SessionPhase.Starting || Session.phase === SessionPhase.Connecting

        Column {
            anchors.centerIn: parent
            spacing: units.gu(3)

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: units.gu(12)
                height: width
                source: "qrc:/assets/logo.svg"
                sourceSize.width: width * 2
                sourceSize.height: height * 2
            }

            ActivityIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: parent.visible
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Session.connection.networkOnline ? i18n.tr("Connecting to Discord...") : i18n.tr("Waiting for network...")
                color: theme.palette.normal.backgroundSecondaryText
            }
        }
    }

    // Short, non-blocking notices (errors from Discord, send failures).
    Rectangle {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: units.gu(10)
        }
        z: 4
        width: Math.min(parent.width - units.gu(4), noticeLabel.implicitWidth + units.gu(4))
        height: noticeLabel.implicitHeight + units.gu(2)
        radius: units.gu(1)
        color: theme.palette.normal.overlay
        border.color: theme.palette.normal.base
        visible: Session.noticeText !== ""

        Label {
            id: noticeLabel
            anchors.centerIn: parent
            width: parent.width - units.gu(4)
            text: Session.noticeText
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        MouseArea {
            anchors.fill: parent
            onClicked: Session.clearNotice()
        }
    }
}
