import QtQuick
import Lomiri.Components

// A server or DM icon in the Lomiri shape; initials or a Suru icon when
// there is no image.
Item {
    id: icon

    property url imageSource: ""
    property string iconName: ""
    property string label: ""
    property bool highlighted: false

    width: units.gu(5)
    height: units.gu(5)

    LomiriShape {
        anchors.fill: parent
        aspect: LomiriShape.DropShadow
        radius: "medium"
        backgroundColor: icon.highlighted || icon.imageSource == ""
                         ? theme.palette.highlighted.base
                         : theme.palette.normal.base
        sourceFillMode: LomiriShape.PreserveAspectCrop
        source: icon.imageSource != "" ? image : null

        Image {
            id: image
            visible: false
            source: icon.imageSource
            asynchronous: true
            sourceSize.width: units.gu(10)
            sourceSize.height: units.gu(10)
        }
    }

    Icon {
        anchors.centerIn: parent
        width: units.gu(2.5)
        height: width
        name: icon.iconName
        visible: icon.imageSource == "" && icon.iconName !== ""
        color: icon.highlighted ? "white" : theme.palette.normal.backgroundText
    }

    Label {
        anchors.centerIn: parent
        text: icon.label
        visible: icon.imageSource == "" && icon.iconName === ""
        font.pixelSize: units.gu(1.6)
        font.bold: true
        color: "white"
    }
}
