import QtQuick
import Lomiri.Components
import Disports.Core

// Full-screen viewer for photos, GIFs and videos, with a button to open the
// original in the browser.
Page {
    id: viewer
    objectName: "mediaViewerPage"

    // An entry of a message's `media`, see MessageContent::media().
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

    // Photos: pinch or double-tap to zoom, drag to pan. Zooming resizes the
    // content about the point between the fingers (or the tapped one), so
    // that point stays put, as in Qt's photo viewer example.
    Component {
        id: imageView

        Flickable {
            id: flick

            readonly property real maxZoom: 6
            readonly property bool zoomed: contentWidth > width + 1

            contentWidth: width
            contentHeight: height
            clip: true

            // Zooms to `zoom` (1: fitted) about `center`, in content coordinates.
            function zoomAbout(zoom, center) {
                const z = Math.max(1, Math.min(maxZoom, zoom))
                resizeContent(width * z, height * z, center)
                returnToBounds()
            }

            function resetZoom() {
                resizeContent(width, height, Qt.point(0, 0))
                contentX = 0
                contentY = 0
            }

            // A new size (rotation): start over, fitted.
            onWidthChanged: resetZoom()
            onHeightChanged: resetZoom()

            PinchArea {
                id: pinchArea
                width: Math.max(flick.contentWidth, flick.width)
                height: Math.max(flick.contentHeight, flick.height)

                property real startWidth: 0

                onPinchStarted: startWidth = flick.contentWidth
                onPinchUpdated: function(pinch) {
                    // Follow the fingers as they move, and scale about them.
                    flick.contentX += pinch.previousCenter.x - pinch.center.x
                    flick.contentY += pinch.previousCenter.y - pinch.center.y
                    const zoom = Math.max(1, Math.min(flick.maxZoom, startWidth * pinch.scale / flick.width))
                    flick.resizeContent(flick.width * zoom, flick.height * zoom, pinch.center)
                }
                onPinchFinished: flick.returnToBounds()

                Image {
                    id: photo
                    width: flick.contentWidth
                    height: flick.contentHeight
                    source: viewer.media.viewUrl
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    Component.onCompleted: busy.running = Qt.binding(function() { return photo.status === Image.Loading })

                    MouseArea {
                        anchors.fill: parent
                        onDoubleClicked: function(mouse) {
                            if (flick.zoomed)
                                flick.resetZoom()
                            else
                                flick.zoomAbout(2.5, Qt.point(mouse.x, mouse.y))
                        }
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
                text: i18n.tr("Videos can't be played on this device.") + " " + i18n.tr("Use \"Open in browser\".")
                color: "white"
            }
        }
    }
}
