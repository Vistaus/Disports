import QtQuick
import Lomiri.Components

// A picture in a LomiriShape, for big pictures (calls), or `label` (the
// initials) on a tile when there is none. `ring` outlines it
// (someone talking): the same shape, shifted outwards in 16 directions
// behind the picture, so the ring is as wide everywhere.
Item {
    id: picture

    property url source
    property string label
    property bool ring: false
    property real ringWidth: units.gu(0.5)
    property color ringColor: theme.palette.normal.positive

    Repeater {
        model: picture.ring ? 16 : 0

        LomiriShape {
            width: picture.width
            height: picture.height
            x: picture.ringWidth * Math.cos(index * Math.PI / 8)
            y: picture.ringWidth * Math.sin(index * Math.PI / 8)
            aspect: LomiriShape.Flat
            radius: "large"
            backgroundColor: picture.ringColor
        }
    }

    LomiriShape {
        id: shape
        anchors.fill: parent
        aspect: LomiriShape.Flat
        radius: "large"
        backgroundColor: hasImage ? theme.palette.normal.base : theme.palette.highlighted.base
        sourceFillMode: LomiriShape.PreserveAspectCrop
        readonly property bool hasImage: picture.source != "" && image.status !== Image.Error
        source: Image {
            id: image
            source: picture.source
            sourceSize.width: 256
            sourceSize.height: 256
            asynchronous: true
        }
    }

    Label {
        anchors.centerIn: parent
        visible: !shape.hasImage
        text: picture.label
        font.pixelSize: Math.round(picture.height * 0.32)
        font.bold: true
        color: "white"
    }
}
