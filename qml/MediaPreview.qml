import QtQuick
import Lomiri.Components
import Disports.Core

// One attachment or media embed in the chat. A still picture by default:
// GIFs show a badge and videos a play button, and nothing plays until
// opened. With "Play GIFs in the chat" on, GIFs play while `playing` is set
// (the message is on screen).
Item {
    id: preview

    // An entry of a message's `media`, see MessageListModel::mediaOf.
    property var media: ({})
    property real maxWidth: units.gu(30)
    // Set by the message while it is on screen and the app is in front.
    property bool playing: false

    signal opened(var media)

    readonly property bool isFile: media.kind === "file"
    readonly property bool isGif: media.kind === "gif"
    readonly property bool animate: playing && isGif && Session.preferences.autoplayGifs
    readonly property real maxHeight: units.gu(30)
    readonly property real aspect: media.width > 0 && media.height > 0 ? media.height / media.width : 0.75

    // Sized from the dimensions Discord reports, so the row never changes
    // height while the picture loads.
    width: isFile ? maxWidth
                  : Math.min(maxWidth, media.width > 0 ? media.width : maxWidth, maxHeight / aspect)
    height: isFile ? units.gu(5) : width * aspect

    Image {
        id: still
        visible: false
        source: preview.isFile ? "" : (preview.media.previewUrl || "")
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        sourceSize.width: Math.round(preview.width * 1.5)
    }

    // GIF files, animated
    Loader {
        id: animatedLoader
        active: preview.animate && preview.media.viewType === "animated"
        sourceComponent: AnimatedImage {
            visible: false
            source: preview.media.animatedUrl || preview.media.previewUrl
            asynchronous: true
            playing: true
            cache: false
        }
    }

    // GIFs that Discord serves as short videos (Tenor and co.)
    Loader {
        id: videoLoader
        width: preview.width
        height: preview.height
        active: preview.animate && preview.media.viewType === "video" && (preview.media.viewUrl || "") !== ""
                && Session.videoPlaybackAvailable()
        source: active ? Qt.resolvedUrl("GifVideo.qml") : ""
        onLoaded: item.source = preview.media.viewUrl
    }

    readonly property var animatedSource: {
        if (animatedLoader.item && animatedLoader.item.status === Image.Ready)
            return animatedLoader.item
        if (videoLoader.item && videoLoader.item.ready)
            return videoLoader.item.texture
        return null
    }

    LomiriShape {
        anchors.fill: parent
        aspect: LomiriShape.Flat
        radius: "small"
        backgroundColor: theme.palette.normal.base
        sourceFillMode: LomiriShape.PreserveAspectCrop
        source: preview.isFile ? null
              : preview.animatedSource ? preview.animatedSource
              : still.status === Image.Ready ? still : null
    }

    ActivityIndicator {
        anchors.centerIn: parent
        running: !preview.isFile && still.status === Image.Loading
        visible: running
    }

    // GIF badge
    LomiriShape {
        visible: preview.isGif && !preview.animatedSource
        anchors { left: parent.left; top: parent.top; margins: units.gu(0.75) }
        width: gifLabel.implicitWidth + units.gu(1)
        height: gifLabel.implicitHeight + units.gu(0.4)
        aspect: LomiriShape.Flat
        radius: "small"
        backgroundColor: Qt.rgba(0, 0, 0, 0.6)

        Label {
            id: gifLabel
            anchors.centerIn: parent
            text: "GIF"
            textSize: Label.XSmall
            font.bold: true
            color: "white"
        }
    }

    // Play button on videos, and on GIFs that are not playing
    Rectangle {
        visible: preview.media.kind === "video" || (preview.isGif && !preview.animatedSource)
        anchors.centerIn: parent
        width: units.gu(6)
        height: width
        radius: width / 2
        color: Qt.rgba(0, 0, 0, 0.55)

        Icon {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: units.dp(2)
            width: units.gu(3)
            height: width
            name: "media-playback-start"
            color: "white"
        }
    }

    // Other files
    LomiriShape {
        visible: preview.isFile
        anchors.fill: parent
        aspect: LomiriShape.Flat
        radius: "small"
        backgroundColor: theme.palette.normal.base

        Row {
            anchors { left: parent.left; leftMargin: units.gu(1); verticalCenter: parent.verticalCenter }
            spacing: units.gu(1)

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                width: units.gu(2.5)
                height: width
                name: "attachment"
                color: theme.palette.normal.backgroundText
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                width: preview.width - units.gu(5)
                text: preview.media.fileName || ""
                elide: Text.ElideMiddle
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: preview.opened(preview.media)
    }
}
