import QtQuick
import Lomiri.Components
import Disports.Core

// The call screen: everyone's picture, ringed in green while talking, and
// the dialer's controls. Incoming calls look like the dialer's.
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

    // An icon that toggles, as in the dialer's call controls.
    component ControlButton: AbstractButton {
        id: control
        property string iconName

        width: units.gu(7)
        height: units.gu(7)
        opacity: !enabled ? 0.2 : pressed ? 0.5 : 1

        Icon {
            anchors.centerIn: parent
            width: units.gu(3)
            height: width
            name: control.iconName
            color: theme.palette.normal.baseText
        }
    }

    // Everyone's state on their picture: deafened, else muted.
    component StateBadge: LomiriShape {
        property string iconName

        width: units.gu(3.5)
        height: width
        aspect: LomiriShape.Flat
        radius: "large"
        backgroundColor: theme.palette.normal.negative

        Icon {
            anchors.centerIn: parent
            width: units.gu(2)
            height: width
            name: parent.iconName
            color: "white"
        }
    }

    // Answer / decline, wide like the dialer's.
    component WideCallButton: AbstractButton {
        id: wide
        property string iconName
        property color color
        property color iconColor

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
            color: wide.iconColor
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

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: callPage.pageStack.push(Qt.resolvedUrl("ProfilePage.qml"),
                                                                           { "userId": modelData.id })
                                    }
                                }

                                StateBadge {
                                    visible: modelData.deafened || modelData.muted
                                    anchors { right: parent.right; bottom: parent.bottom }
                                    iconName: modelData.deafened ? "system-suspend" : "microphone-mute"
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
                                visible: !modelData.joined && (modelData.self || modelData.ringing)
                                text: modelData.self ? i18n.tr("Connecting...") : i18n.tr("Calling...")
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
            label: callPage.call.initials
        }
    }

    Column {
        id: controls
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: units.gu(4) }
        spacing: units.gu(2)
        visible: !callPage.incoming

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: callPage.call.deafened
            spacing: units.gu(0.75)

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                width: units.gu(2)
                height: width
                name: "system-suspend"
                color: theme.palette.normal.backgroundSecondaryText
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: i18n.tr("You are deafened")
                textSize: Label.Small
                color: theme.palette.normal.backgroundSecondaryText
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: units.gu(2)

            ControlButton {
                iconName: callPage.call.muted ? "microphone-mute" : "microphone"
                onClicked: callPage.call.toggleMute()
            }

            ControlButton {
                iconName: callPage.call.speaker ? "speaker" : "speaker-mute"
                enabled: callPage.call.speakerAvailable
                onClicked: callPage.call.toggleSpeaker()
            }

            ControlButton {
                iconName: callPage.call.deafened ? "system-suspend" : "media-preview-start"
                onClicked: callPage.call.toggleDeafen()
            }
        }

        AbstractButton {
            anchors.horizontalCenter: parent.horizontalCenter
            width: units.gu(21)
            height: units.gu(4.5)
            onClicked: callPage.call.hangUp()

            LomiriShape {
                anchors.fill: parent
                aspect: LomiriShape.Flat
                radius: "medium"
                backgroundColor: parent.pressed ? theme.palette.highlighted.negative : theme.palette.normal.negative
            }

            Icon {
                anchors.centerIn: parent
                width: units.gu(3)
                height: width
                name: "call-end"
                color: theme.palette.normal.negativeText
            }
        }
    }

    Item {
        id: incomingControls
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: units.gu(2) }
        height: units.gu(8)
        visible: callPage.incoming

        Row {
            anchors.centerIn: parent
            spacing: units.gu(2)

            WideCallButton {
                width: units.gu(16)
                iconName: "call-end"
                color: theme.palette.normal.negative
                iconColor: theme.palette.normal.negativeText
                onClicked: callPage.call.decline()
            }

            WideCallButton {
                width: units.gu(16)
                iconName: "call-start"
                color: theme.palette.normal.positive
                iconColor: theme.palette.normal.positiveText
                onClicked: callPage.call.accept()
            }
        }
    }
}
