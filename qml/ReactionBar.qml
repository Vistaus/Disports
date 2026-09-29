import QtQuick
import Lomiri.Components
import Disports.Core

// The reactions under a message. Tapping one adds or removes yours; the
// last chip opens the emoji picker to add another.
Flow {
    id: bar

    property string messageId: ""
    property var reactions: []

    signal addRequested()

    spacing: units.gu(0.5)

    Repeater {
        model: bar.reactions

        delegate: AbstractButton {
            id: chip

            required property var modelData

            width: content.width + units.gu(1.5)
            height: units.gu(3.2)
            onClicked: Session.toggleReaction(bar.messageId, modelData.emoji, modelData.me)

            LomiriShape {
                anchors.fill: parent
                aspect: LomiriShape.Flat
                radius: "small"
                backgroundColor: chip.modelData.me ? theme.palette.normal.focus : theme.palette.normal.base
            }

            Row {
                id: content
                anchors.centerIn: parent
                spacing: units.gu(0.5)

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: chip.modelData.imageUrl === ""
                    text: chip.modelData.text
                    font.pixelSize: units.gu(1.9)
                }

                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: chip.modelData.imageUrl !== ""
                    width: units.gu(2.2)
                    height: width
                    source: chip.modelData.imageUrl
                    sourceSize.width: units.gu(4)
                    sourceSize.height: units.gu(4)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: chip.modelData.count
                    textSize: Label.Small
                    font.bold: chip.modelData.me
                    color: chip.modelData.me ? theme.palette.normal.focusText : theme.palette.normal.backgroundText
                }
            }
        }
    }

    AbstractButton {
        id: addChip
        visible: bar.reactions.length > 0
        width: units.gu(4)
        height: units.gu(3.2)
        onClicked: bar.addRequested()

        LomiriShape {
            anchors.fill: parent
            aspect: LomiriShape.Flat
            radius: "small"
            backgroundColor: theme.palette.normal.base
        }

        Icon {
            anchors.centerIn: parent
            width: units.gu(1.8)
            height: width
            name: "add"
            color: theme.palette.normal.backgroundText
        }
    }
}
