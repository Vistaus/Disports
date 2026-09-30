import QtQuick
import Lomiri.Components
import Disports.Core

// The file going with the next message, or the one being sent.
Rectangle {
    property string fileName

    signal closed()

    height: fileName !== "" || Session.sender.uploading ? units.gu(5) : 0
    visible: height > 0
    color: theme.palette.normal.base

    Icon {
        id: icon
        anchors { left: parent.left; leftMargin: units.gu(2); verticalCenter: parent.verticalCenter }
        width: units.gu(2.5)
        height: width
        name: "attachment"
        color: theme.palette.normal.backgroundText
    }

    Column {
        anchors {
            left: icon.right
            right: close.left
            leftMargin: units.gu(1.5)
            rightMargin: units.gu(1.5)
            verticalCenter: parent.verticalCenter
        }
        spacing: units.gu(0.5)

        Label {
            width: parent.width
            text: Session.sender.uploading ? i18n.tr("Sending %1").arg(Session.sender.uploadName) : fileName
            font.pixelSize: units.gu(1.4)
            elide: Text.ElideMiddle
        }

        ProgressBar {
            width: parent.width
            height: units.gu(0.5)
            visible: Session.sender.uploading
            minimumValue: 0
            maximumValue: 1
            value: Session.sender.uploadProgress
            showProgressPercentage: false
        }
    }

    Icon {
        id: close
        anchors { right: parent.right; rightMargin: units.gu(2); verticalCenter: parent.verticalCenter }
        width: units.gu(2)
        height: width
        name: "close"
        color: theme.palette.normal.backgroundText

        MouseArea {
            anchors.fill: parent
            anchors.margins: -units.gu(1)
            onClicked: Session.sender.uploading ? Session.sender.cancelUpload() : closed()
        }
    }
}
