import QtQuick
import Lomiri.Components
import Disports.Core

// The media viewer's video player: tap to pause or resume. GIFs served as
// video loop muted.
Item {
    id: videoPlayer

    property url source
    property bool looping: false

    readonly property bool loading: !player.hasFrame && player.errorString === ""

    GstVideoPlayer {
        id: player
        anchors.fill: parent
        fillMode: GstVideoPlayer.PreserveAspectFit
        source: videoPlayer.source
        loops: videoPlayer.looping
        muted: videoPlayer.looping
        autoPlay: true
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (player.playing)
                player.pause()
            else
                player.play()
        }
    }

    Rectangle {
        anchors.centerIn: parent
        visible: !player.playing && !videoPlayer.loading && player.errorString === ""
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
        visible: player.errorString !== ""
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: i18n.tr("The video could not be played.") + " " + i18n.tr("Use \"Open in browser\".")
        color: "white"
    }
}
