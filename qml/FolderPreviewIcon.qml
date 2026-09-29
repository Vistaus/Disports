import QtQuick
import Lomiri.Components

// A collapsed server folder: up to four server icons in a 2×2 grid, in one
// Lomiri shape.
Item {
    id: folderPreview

    property var previews: [] // [{iconUrl, initials}]

    function refresh() {
        previewSource.scheduleUpdate()
    }

    onPreviewsChanged: Qt.callLater(refresh)
    Component.onCompleted: Qt.callLater(refresh)

    ShaderEffectSource {
        id: previewSource
        anchors.centerIn: parent
        width: 0
        height: 0
        sourceItem: previewContent
        live: false
        hideSource: true
    }

    Item {
        id: previewContent
        width: folderPreview.width
        height: folderPreview.height

        Rectangle {
            anchors.fill: parent
            color: theme.palette.normal.base
        }

        Grid {
            id: previewGrid
            anchors.fill: parent
            anchors.margins: units.dp(3)
            spacing: units.dp(2)
            columns: 2

            Repeater {
                model: folderPreview.previews

                delegate: Item {
                    required property var modelData

                    width: (previewGrid.width - previewGrid.spacing) / 2
                    height: (previewGrid.height - previewGrid.spacing) / 2

                    Rectangle {
                        anchors.fill: parent
                        radius: units.dp(3)
                        color: theme.palette.highlighted.base
                        visible: previewImage.status !== Image.Ready

                        Label {
                            anchors.centerIn: parent
                            text: modelData.initials
                            font.pixelSize: Math.round(parent.height * 0.45)
                            font.bold: true
                            color: "white"
                        }
                    }

                    Image {
                        id: previewImage
                        anchors.fill: parent
                        source: modelData.iconUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize.width: units.gu(5)
                        sourceSize.height: units.gu(5)
                        onStatusChanged: {
                            if (status === Image.Ready || status === Image.Error)
                                folderPreview.refresh()
                        }
                    }
                }
            }
        }
    }

    LomiriShape {
        anchors.fill: parent
        aspect: LomiriShape.DropShadow
        radius: "medium"
        source: previewSource
    }
}
