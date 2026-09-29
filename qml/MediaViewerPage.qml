import QtQuick
import Lomiri.Components
import Disports.Core

// Full-screen viewer for photos, GIFs and videos, with a button to open the
// original in the browser.
Page {
    id: viewer
    objectName: "mediaViewerPage"

    // An entry of a message's `media`, see MessageListModel::mediaOf.
    property var media: ({})

    readonly property string viewType: media.viewType || "none"
    readonly property bool looping: media.kind === "gif"

    header: PageHeader {
        title: viewer.media.fileName || (viewer.media.kind === "gif" ? "GIF" : i18n.tr("Media"))
        trailingActionBar.actions: [
            Action {
                iconName: "external-link"
                text: i18n.tr("Open in browser")
                onTriggered: Qt.openUrlExternally(viewer.media.openUrl || viewer.media.viewUrl)
            }
        ]
    }

    Rectangle {
        anchors {
            top: viewer.header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        color: "black"

        Loader {
            anchors.fill: parent
            sourceComponent: {
                if (viewer.viewType === "video")
                    return videoView
                if (viewer.viewType === "animated")
                    return animatedView
                if (viewer.viewType === "image")
                    return imageView
                return null
            }
        }

        ActivityIndicator {
            id: busy
            anchors.centerIn: parent
            running: false
        }
    }

    // Photos: pinch or double-tap to zoom, drag to pan.
    Component {
        id: imageView

        Flickable {
            id: flick

            contentWidth: Math.max(width, photo.width * photo.scale)
            contentHeight: Math.max(height, photo.height * photo.scale)
            clip: true

            function resetZoom() {
                photo.scale = 1
                flick.contentX = 0
                flick.contentY = 0
            }

            Image {
                id: photo
                width: flick.width
                height: flick.height
                x: (flick.contentWidth - width) / 2
                y: (flick.contentHeight - height) / 2
                source: viewer.media.viewUrl
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                Component.onCompleted: busy.running = Qt.binding(function() { return photo.status === Image.Loading })
            }

            PinchArea {
                width: flick.contentWidth
                height: flick.contentHeight
                pinch.target: photo
                pinch.minimumScale: 1
                pinch.maximumScale: 6
                onPinchFinished: flick.returnToBounds()

                MouseArea {
                    anchors.fill: parent
                    onDoubleClicked: {
                        if (photo.scale > 1)
                            flick.resetZoom()
                        else
                            photo.scale = 2.5
                    }
                }
            }
        }
    }

    // Animated GIF files
    Component {
        id: animatedView

        AnimatedImage {
            source: viewer.media.viewUrl
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            playing: true
            Component.onCompleted: busy.running = Qt.binding(function() { return status === Image.Loading })
        }
    }

    // Videos, and GIFs that Discord serves as video.
    Component {
        id: videoView

        Item {
            readonly property bool canPlay: Session.videoPlaybackAvailable()

            Loader {
                id: playerLoader
                anchors.fill: parent
                active: parent.canPlay
                source: Qt.resolvedUrl("VideoPlayer.qml")
                onLoaded: {
                    item.looping = viewer.looping
                    item.source = viewer.media.viewUrl
                }
            }

            Binding {
                target: busy
                property: "running"
                value: playerLoader.item ? playerLoader.item.loading : playerLoader.status === Loader.Loading
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - units.gu(4)
                visible: !parent.canPlay || playerLoader.status === Loader.Error
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: i18n.tr("Videos can't be played on this device yet. Use \"Open in browser\".")
                color: "white"
            }
        }
    }
}
