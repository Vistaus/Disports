import QtQuick
import Lomiri.Components
import Lomiri.Content

// Picks a file through Content Hub. `picked(url, transfer)` gives a
// file:// URL to a copy; finalize the transfer once it has been read to
// remove the copy.
Page {
    id: picker

    signal picked(string url, var transfer)

    property var activeTransfer: null

    // One header: ours (its back button cancels), not the picker's too.
    header: PageHeader {
        title: i18n.tr("Attach a file")
    }

    ContentPeerPicker {
        anchors {
            top: picker.header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        showTitle: false
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
