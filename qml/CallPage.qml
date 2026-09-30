import QtQuick
import Lomiri.Components
import Disports.Core

// The call screen: the pictures of everyone in the call (us included,
// ringed in green while talking), as on Discord, with Lomiri's shapes and
// Suru icons and the dialer's round controls. An incoming call
// looks like the dialer's: the caller's picture, decline and answer.
Page {
    id: callPage
    objectName: "callPage"

    readonly property var call: Session.call
    readonly property bool incoming: call.state === CallState.Incoming
    readonly property var people: call.participants

    header: PageHeader {
        title: callPage.call.title
        subtitle: callPage.call.statusText
    }

    // Round button with a caption, like the dialer's.
    component CallButton: AbstractButton {
        id: button
        property string iconName
        property string caption
        property bool checked: false
        property color color: checked ? theme.palette.normal.backgroundText : theme.palette.normal.base
        property color iconColor: checked ? theme.palette.normal.background : theme.palette.normal.backgroundText

        width: units.gu(8)
        height: units.gu(9)

        LomiriShape {
            id: circle
            anchors.horizontalCenter: parent.horizontalCenter
            width: units.gu(6.5)
            height: width
            aspect: LomiriShape.Flat
            radius: "large"
            backgroundColor: button.color
            opacity: button.pressed ? 0.8 : 1

            Icon {
                anchors.centerIn: parent
                width: units.gu(3)
                height: width
                name: button.iconName
                color: button.iconColor
            }
        }

        Label {
            anchors { top: circle.bottom; topMargin: units.gu(0.5); horizontalCenter: parent.horizontalCenter }
            text: button.caption
            textSize: Label.Small
            color: theme.palette.normal.backgroundSecondaryText
        }
    }

    // Answer / decline, wide like the dialer's.
    component WideCallButton: AbstractButton {
        id: wide
        property string iconName
        property color color

        height: units.gu(6)

        LomiriShape {
            anchors.fill: parent
            aspect: LomiriShape.Flat
            radius: "medium"
            backgroundColor: wide.color
            opacity: wide.pressed ? 0.8 : 1
        }

        Icon {
            anchors.centerIn: parent
            width: units.gu(3)
            height: width
            name: wide.iconName
            color: "white"
        }
    }

    // Everyone in the call: pictures side by side, centred. When the row
    // would take more than 70% of the width, they are spread evenly over
    // several rows instead. A green ring around whoever is talking.
    Flickable {
        id: stage
        visible: !callPage.incoming
        anchors {
            top: callPage.header.bottom
            left: parent.left
            right: parent.right
            bottom: controls.top
            margins: units.gu(1)
        }
        clip: true
        contentHeight: rows.height

        readonly property int count: callPage.people.length
        readonly property real pictureSize: count <= 2 ? units.gu(13) : units.gu(10)
        readonly property real gap: units.gu(3)
        readonly property bool oneRow: count * pictureSize + (count - 1) * gap <= width * 0.7
        // Evenly spread: no more per row than fit in 70% of the width, and
        // the rows balanced (4 as 2 + 2, not 3 + 1).
        readonly property int maxPerRow: Math.max(1, Math.floor((width * 0.7 + gap) / (pictureSize + gap)))
        readonly property int perRow: oneRow ? Math.max(1, count)
            : Math.ceil(count / Math.ceil(count / maxPerRow))
        readonly property real cellWidth: pictureSize + gap

        function chunks(list) {
            const out = []
            for (let i = 0; i < list.length; i += perRow)
                out.push(list.slice(i, i + perRow))
            return out
        }

        Column {
            id: rows
            width: stage.width
            y: Math.max(0, (stage.height - height) / 2)
            spacing: units.gu(2)

            Repeater {
                model: stage.chunks(callPage.people)

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter

                    Repeater {
                        model: modelData

                        Column {
                            width: stage.cellWidth
                            spacing: units.gu(1)
                            opacity: modelData.joined ? 1 : 0.5

                            Item {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: stage.pictureSize
                                height: width

                                RoundedPicture {
                                    anchors.fill: parent
                                    source: modelData.avatarUrl
                                    ring: modelData.speaking
                                }

                                // Muted: a badge in the corner.
                                LomiriShape {
                                    visible: modelData.muted
                                    anchors { right: parent.right; bottom: parent.bottom }
                                    width: units.gu(3.5)
                                    height: width
                                    aspect: LomiriShape.Flat
                                    radius: "large"
                                    backgroundColor: theme.palette.normal.negative

                                    Icon {
                                        anchors.centerIn: parent
                                        width: units.gu(2)
                                        height: width
                                        name: "microphone-mute"
                                        color: "white"
                                    }
                                }
                            }

                            Label {
                                width: parent.width - units.gu(1)
                                anchors.horizontalCenter: parent.horizontalCenter
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData.name
                                elide: Text.ElideRight
                            }

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: !modelData.joined
                                text: modelData.self ? i18n.tr("Connecting…") : i18n.tr("Calling…")
                                textSize: Label.Small
                                color: theme.palette.normal.backgroundSecondaryText
                            }
                        }
                    }
                }
            }
        }
    }

    // Incoming: the caller, big.
    Column {
        visible: callPage.incoming
        anchors { centerIn: parent; verticalCenterOffset: -units.gu(6) }
        spacing: units.gu(2)

        RoundedPicture {
            anchors.horizontalCenter: parent.horizontalCenter
            width: units.gu(18)
            height: width
            source: callPage.call.avatarUrl
        }
    }

    Item {
        id: controls
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: units.gu(2) }
        height: callPage.incoming ? units.gu(8) : units.gu(9)

        Row {
            anchors.centerIn: parent
            visible: !callPage.incoming
            spacing: units.gu(0.5)

            CallButton {
                iconName: callPage.call.muted ? "microphone-mute" : "microphone"
                caption: i18n.tr("Mute")
                checked: callPage.call.muted
                // No "Speak" permission: muted for good (tapping says why).
                opacity: callPage.call.canSpeak ? 1 : 0.5
                onClicked: callPage.call.toggleMute()
            }

            CallButton {
                iconName: "speaker-mute"
                caption: i18n.tr("Deafen")
                checked: callPage.call.deafened
                onClicked: callPage.call.toggleDeafen()
            }

            CallButton {
                iconName: "speaker"
                caption: i18n.tr("Speaker")
                visible: callPage.call.speakerAvailable
                checked: callPage.call.speaker
                onClicked: callPage.call.toggleSpeaker()
            }

            CallButton {
                iconName: "call-end"
                caption: i18n.tr("Leave")
                color: theme.palette.normal.negative
                iconColor: "white"
                onClicked: callPage.call.hangUp()
            }
        }

        Row {
            anchors.centerIn: parent
            visible: callPage.incoming
            spacing: units.gu(2)

            WideCallButton {
                width: units.gu(16)
                iconName: "call-end"
                color: theme.palette.normal.negative
                onClicked: callPage.call.decline()
            }

            WideCallButton {
                width: units.gu(16)
                iconName: "call-start"
                color: theme.palette.normal.positive
                onClicked: callPage.call.accept()
            }
        }
    }
}
