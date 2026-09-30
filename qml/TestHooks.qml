import QtQuick
import Lomiri.Components
import Disports.Core

// Test builds only: the QML side of src/testing/TestHooks.h.
Item {
    property var stack
    property var root

    // DISPORTS_OPEN_CHANNEL on a phone-sized window: show the chat page too.
    Connections {
        target: Session
        function onCurrentChannelChanged() {
            if (testHooks.openChannel !== "" && Session.currentChannelId === testHooks.openChannel
                    && !root.wideLayout && root.currentPageName === "mainPage")
                stack.push(Qt.resolvedUrl("ChatPage.qml"))
        }
    }

    // DISPORTS_PLAY_VIDEO: "<url>" in the media viewer, "gif:<url>" as a
    // looping GIF there, "preview:<url>" as the chat's preview of a GIF.
    Timer {
        running: testHooks.videoUrl !== ""
        interval: 1500
        onTriggered: {
            const spec = testHooks.videoUrl
            if (spec.startsWith("preview:")) {
                stack.push(previewPage, { "url": spec.substring(8) })
                return
            }
            const gif = spec.startsWith("gif:")
            const url = gif ? spec.substring(4) : spec
            stack.push(Qt.resolvedUrl("MediaViewerPage.qml"), { "media": {
                "kind": gif ? "gif" : "video", "viewType": "video", "viewUrl": url, "openUrl": url } })
        }
    }

    Component {
        id: previewPage

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
}
