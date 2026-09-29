import QtQuick
import Lomiri.Components
import Disports.Core

MainView {
    id: root

    applicationName: "disports.jukfiuu"
    // MainView's own keyboard handling uses the keyboard height as reported,
    // which on Ubuntu Touch with Qt 6 is not HiDPI-scaled: it overshoots and
    // pushes the whole app off screen. Keep room for the keyboard ourselves
    // (the same workaround Morph uses).
    anchorToKeyboard: false
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

    PageStack {
        id: stack
        anchors.fill: parent
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
                text: Session.networkOnline ? i18n.tr("Connecting to Discord…") : i18n.tr("Waiting for network…")
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
