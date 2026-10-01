import QtQuick
import Lomiri.Components

// A collapsed server folder: up to four server icons in a 2x2 grid, filling
// one Lomiri shape (which rounds the outer corners).
Item {
    id: folderPreview

    property var previews: [] // [{iconUrl, initials}]
    property bool shadow: true

    function refresh() {
        previewSource.scheduleUpdate()
    }

    onPreviewsChanged: Qt.callLater(refresh)
    onShadowChanged: Qt.callLater(refresh)
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

        // Selected (no shadow): set off from the highlighted row.
        Rectangle {
            anchors.fill: parent
            color: folderPreview.shadow ? theme.palette.normal.base : theme.palette.normal.background
        }

        Grid {
            id: previewGrid
            anchors.fill: parent
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
        aspect: folderPreview.shadow ? LomiriShape.DropShadow : LomiriShape.Flat
        radius: "medium"
        source: previewSource
    }
}
