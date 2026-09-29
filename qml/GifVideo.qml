import QtQuick
import QtMultimedia

// A GIF that Discord serves as a short video (Tenor and co.), playing muted
// in a loop. Loaded by MediaPreview through a Loader, so the chat still works
// when QtMultimedia is missing.
Item {
    id: gifVideo

    property url source

    // Only once frames arrive; until then (or if the video fails) the
    // preview keeps its still picture.
    readonly property bool ready: video.playbackState === MediaPlayer.PlayingState
                                  && video.position > 0 && video.error === MediaPlayer.NoError
    readonly property alias texture: videoTexture

    Video {
        id: video
        anchors.fill: parent
        source: gifVideo.source
        fillMode: VideoOutput.PreserveAspectCrop
        muted: true
        loops: MediaPlayer.Infinite
        autoPlay: true
    }

    // LomiriShape needs a texture provider to round the corners.
    ShaderEffectSource {
        id: videoTexture
        sourceItem: video
        hideSource: true
        live: true
        visible: false
    }
}
