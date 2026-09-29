import QtQuick
import Lomiri.Components
import Disports.Core

// A sticker. GIF stickers play like GIFs (only with "Play GIFs in the chat"
// on); Lottie stickers, which Qt cannot draw, show their name.
Item {
    id: stickerView

    // An entry of a message's `stickers`, see MessageListModel::stickersOf.
    property var sticker: ({})
    property bool playing: false

    readonly property bool animate: playing && sticker.animated && Session.preferences.autoplayGifs

    width: units.gu(16)
    height: units.gu(16)

    Image {
        anchors.fill: parent
        visible: !stickerView.sticker.lottie && !animated.visible
        source: stickerView.sticker.lottie ? "" : (stickerView.sticker.url || "")
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        sourceSize.width: units.gu(32)
    }

    Loader {
        id: animated
        anchors.fill: parent
        active: stickerView.animate
        visible: active && item && item.status === Image.Ready
        sourceComponent: AnimatedImage {
            source: stickerView.sticker.url
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            playing: true
        }
    }

    LomiriShape {
        visible: !!stickerView.sticker.lottie
        anchors.fill: parent
        aspect: LomiriShape.Flat
        radius: "medium"
        backgroundColor: theme.palette.normal.base

        Column {
            anchors.centerIn: parent
            width: parent.width - units.gu(2)
            spacing: units.gu(0.5)

            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: units.gu(4)
                height: width
                name: "ayatana-indicator-keyboard-emoji"
                color: theme.palette.normal.backgroundSecondaryText
            }

            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: stickerView.sticker.name || ""
                textSize: Label.Small
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                color: theme.palette.normal.backgroundSecondaryText
            }
        }
    }
}
