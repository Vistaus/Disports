import QtQuick
import Disports.Core

// A GIF that Discord serves as a short video (Tenor and co.), playing muted
// in a loop.
Item {
    id: gifVideo

    property url source

    // Only once frames arrive; until then (or if the video fails) the
    // preview keeps its still picture.
    readonly property bool ready: player.hasFrame && player.errorString === ""
    readonly property alias texture: videoTexture

    GstVideoPlayer {
        id: player
        anchors.fill: parent
        fillMode: GstVideoPlayer.PreserveAspectCrop
        source: gifVideo.source
        muted: true
        loops: true
        autoPlay: true
    }

    // LomiriShape needs a texture provider to round the corners.
    ShaderEffectSource {
        id: videoTexture
        sourceItem: player
        hideSource: true
        live: true
        visible: false
    }
}
