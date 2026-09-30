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

    Component.onCompleted: {
        showPhase()
        if (testVideoUrl !== "")
            testVideoTimer.start()
    }

    Component {
        id: testPreviewPage

        Page {
            property string url
            header: PageHeader { title: "Preview" }

            MediaPreview {
                anchors.centerIn: parent
                maxWidth: parent.width - units.gu(4)
                playing: true
                media: ({ "kind": "gif", "viewType": "video", "viewUrl": parent.url, "previewUrl": "",
                          "width": 498, "height": 374 })
            }
        }
    }

    // Test hook, see DISPORTS_PLAY_VIDEO in main.cpp.
    Timer {
        id: testVideoTimer
        interval: 1500
        // "gif:<url>" opens it as a GIF (looping, muted); "preview:<url>"
        // shows the chat's preview of such a GIF instead.
        onTriggered: {
            if (testVideoUrl.startsWith("preview:")) {
                stack.push(testPreviewPage, { "url": testVideoUrl.substring(8) })
                return
            }
            const gif = testVideoUrl.startsWith("gif:")
            const url = gif ? testVideoUrl.substring(4) : testVideoUrl
            stack.push(Qt.resolvedUrl("MediaViewerPage.qml"), { "media": {
                "kind": gif ? "gif" : "video", "viewType": "video", "viewUrl": url, "openUrl": url } })
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

    // Offline / reconnecting: above everything, the page title included.
    ConnectionBanner {
        id: connectionBanner
        anchors { top: parent.top; left: parent.left; right: parent.right }
        z: 2
    }

    // Something in progress (connecting, loading a conversation): Lomiri's
    // indeterminate progress strip at the top, like the Qt 5 version's.
    ProgressBar {
        anchors { top: connectionBanner.bottom; left: parent.left; right: parent.right }
        z: 2
        indeterminate: true
        visible: Session.networkOnline
                 && ((connectionBanner.reconnecting && Session.reconnectSeconds === 0)
                     || Session.loadingMessages)
    }

    PageStack {
        id: stack
        anchors.fill: parent
        anchors.topMargin: connectionBanner.height
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
