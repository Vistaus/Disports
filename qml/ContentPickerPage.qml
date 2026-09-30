import QtQuick
import Lomiri.Components
import Lomiri.Content

// Picks a file to attach through Content Hub, the way confined apps get
// files on Ubuntu Touch: the user chooses an app (Gallery, Files, Camera…),
// which hands a copy over. `picked(url, transfer)` gives the copy's file://
// URL; the transfer is finalized (its copy removed) once it has been read.
Page {
    id: picker

    signal picked(string url, var transfer)

    property var activeTransfer: null

    // ContentPeerPicker has its own header, with Cancel.
    ContentPeerPicker {
        anchors.fill: parent
        contentType: ContentType.All
        handler: ContentHandler.Source
        onPeerSelected: {
            peer.selectionType = ContentTransfer.Single
            picker.activeTransfer = peer.request()
        }
        onCancelPressed: picker.pageStack.pop()
    }

    ContentTransferHint {
        anchors.fill: parent
        activeTransfer: picker.activeTransfer
    }

    Connections {
        target: picker.activeTransfer
        function onStateChanged() {
            const transfer = picker.activeTransfer
            if (transfer.state === ContentTransfer.Charged) {
                if (transfer.items.length > 0)
                    picker.picked(String(transfer.items[0].url), transfer)
                picker.pageStack.pop()
            } else if (transfer.state === ContentTransfer.Aborted) {
                picker.pageStack.pop()
            }
        }
    }
}
