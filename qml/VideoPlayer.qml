import QtQuick
import QtMultimedia
import Lomiri.Components

// The media viewer's video player: tap to pause or resume. GIFs served as
// video loop muted. Loaded by MediaViewerPage through a Loader, so the viewer
// still opens when QtMultimedia is missing.
Item {
    id: player

    property url source
    property bool looping: false

    readonly property bool loading: video.playbackState !== MediaPlayer.PlayingState
                                    && video.bufferProgress < 1 && video.error === MediaPlayer.NoError

    Video {
        id: video
        anchors.fill: parent
        source: player.source
        fillMode: VideoOutput.PreserveAspectFit
        loops: player.looping ? MediaPlayer.Infinite : 1
        muted: player.looping
        autoPlay: true
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (video.playbackState === MediaPlayer.PlayingState)
                video.pause()
            else
                video.play()
        }
    }

    Rectangle {
        anchors.centerIn: parent
        visible: video.playbackState !== MediaPlayer.PlayingState && !player.loading
                 && video.error === MediaPlayer.NoError
        width: units.gu(8)
        height: width
        radius: width / 2
        color: Qt.rgba(0, 0, 0, 0.55)

        Icon {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: units.dp(3)
            width: units.gu(4)
            height: width
            name: "media-playback-start"
            color: "white"
        }
    }

    Label {
        anchors.centerIn: parent
        width: parent.width - units.gu(4)
        visible: video.error !== MediaPlayer.NoError
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: i18n.tr("This video can't be played here. Use \"Open in browser\".")
        color: "white"
    }
}
