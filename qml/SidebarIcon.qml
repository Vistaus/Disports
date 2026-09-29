import QtQuick
import Lomiri.Components

// A server, DM or folder icon in the Lomiri shape: the image, or a Suru
// icon / initials on a tile when there is none.
Item {
    id: iconItem

    property url imageSource: ""
    property string iconName: ""
    property string label: ""
    property bool showTileBackground: false

    readonly property bool hasImage: imageSource != "" && image.status !== Image.Error

    width: units.gu(5)
    height: units.gu(5)

    Image {
        id: image
        visible: false
        source: iconItem.imageSource
        asynchronous: true
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: units.gu(10)
        sourceSize.height: units.gu(10)
    }

    LomiriShape {
        anchors.fill: parent
        aspect: LomiriShape.DropShadow
        radius: iconItem.width > units.gu(3) ? "medium" : "small"
        backgroundColor: iconItem.showTileBackground || !iconItem.hasImage
                         ? theme.palette.highlighted.base
                         : "transparent"
        sourceFillMode: LomiriShape.PreserveAspectCrop
        source: iconItem.hasImage && image.status === Image.Ready ? image : null
    }

    Icon {
        anchors.centerIn: parent
        width: iconItem.width / 2
        height: width
        name: iconItem.iconName
        visible: !iconItem.hasImage && iconItem.iconName !== ""
        color: "white"
    }

    Label {
        anchors.centerIn: parent
        width: parent.width - units.gu(0.5)
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: iconItem.label
        visible: !iconItem.hasImage && iconItem.iconName === ""
        font.pixelSize: Math.round(iconItem.height * 0.32)
        font.bold: true
        color: "white"
    }
}
