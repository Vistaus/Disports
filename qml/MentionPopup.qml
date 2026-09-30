import QtQuick
import Lomiri.Components

// Who or what the @ / # word being typed could be; see
// MentionSuggester::suggestions() for the entries.
Rectangle {
    property alias suggestions: list.model

    signal picked(string text)

    height: Math.min(list.contentHeight, units.gu(30))
    visible: list.count > 0
    color: theme.palette.normal.background

    Rectangle {
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: units.dp(1)
        color: theme.palette.normal.base
    }

    ListView {
        id: list
        anchors.fill: parent
        clip: true

        delegate: ListItem {
            required property var modelData
            height: layout.height + (divider.visible ? divider.height : 0)
            onClicked: picked(modelData.insert)

            ListItemLayout {
                id: layout
                title.text: modelData.label
                title.color: modelData.color ? modelData.color : theme.palette.normal.backgroundText
                subtitle.text: modelData.detail

                Item {
                    SlotsLayout.position: SlotsLayout.Leading
                    width: units.gu(4)
                    height: width

                    LomiriShape {
                        anchors.fill: parent
                        visible: modelData.kind === "user"
                        aspect: LomiriShape.Flat
                        backgroundColor: theme.palette.normal.base
                        sourceFillMode: LomiriShape.PreserveAspectCrop
                        source: Image {
                            source: modelData.kind === "user" ? modelData.avatarUrl : ""
                            sourceSize.width: units.gu(8)
                            sourceSize.height: units.gu(8)
                            asynchronous: true
                        }
                    }

                    Icon {
                        anchors.centerIn: parent
                        visible: modelData.kind !== "user"
                        width: units.gu(2.5)
                        height: width
                        name: modelData.kind === "channel" ? "message"
                            : modelData.kind === "everyone" ? "notification"
                            : "contact-group"
                        color: theme.palette.normal.backgroundSecondaryText
                    }
                }
            }
        }
    }
}
