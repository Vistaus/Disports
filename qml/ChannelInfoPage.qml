import QtQuick
import Lomiri.Components
import Disports.Core

// About a channel, DM or group: its type, server, topic and, for DMs and
// groups, who is in it.
Page {
    id: infoPage
    objectName: "channelInfoPage"

    property string channelId: ""
    readonly property var info: Session.channelInfo(channelId)
    readonly property var members: info.members || []

    header: PageHeader {
        title: i18n.tr("Info")
    }

    Flickable {
        anchors {
            top: infoPage.header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        contentHeight: column.height
        clip: true

        Column {
            id: column
            width: parent.width

            ListItem {
                height: titleLayout.height + divider.height

                ListItemLayout {
                    id: titleLayout
                    title.text: (infoPage.info.kind === "channel" ? "#" : "") + (infoPage.info.name || "")
                    title.font.bold: true
                    subtitle.text: infoPage.info.typeName || ""

                    SidebarIcon {
                        SlotsLayout.position: SlotsLayout.Leading
                        visible: infoPage.info.kind !== "channel"
                        width: units.gu(5)
                        height: width
                        imageSource: infoPage.info.iconUrl || ""
                        iconName: infoPage.info.kind === "group" && !infoPage.info.iconUrl ? "contact-group" : ""
                        label: (infoPage.info.name || "").charAt(0)
                    }
                }
            }

            ListItem {
                visible: !!infoPage.info.nsfw
                height: visible ? nsfwLayout.height + divider.height : 0

                ListItemLayout {
                    id: nsfwLayout
                    title.text: i18n.tr("Age-restricted channel")
                    title.color: theme.palette.normal.negative

                    Icon {
                        SlotsLayout.position: SlotsLayout.Leading
                        width: units.gu(2.5)
                        height: width
                        name: "security-alert"
                        color: theme.palette.normal.negative
                    }
                }
            }

            ListItem {
                visible: (infoPage.info.topic || "") !== ""
                height: visible ? topicLayout.height + divider.height : 0

                ListItemLayout {
                    id: topicLayout
                    title.text: i18n.tr("Topic")
                    summary.text: infoPage.info.topic || ""
                    summary.wrapMode: Text.Wrap
                    summary.maximumLineCount: 20
                }
            }

            ListItem {
                visible: (infoPage.info.server || "") !== ""
                height: visible ? serverLayout.height + divider.height : 0

                ListItemLayout {
                    id: serverLayout
                    title.text: i18n.tr("Server")
                    subtitle.text: infoPage.info.server || ""
                }
            }

            ListItem {
                visible: (infoPage.info.channel || "") !== ""
                height: visible ? channelLayout.height + divider.height : 0

                ListItemLayout {
                    id: channelLayout
                    title.text: i18n.tr("Channel")
                    subtitle.text: infoPage.info.channel || ""
                }
            }

            ListItem {
                visible: (infoPage.info.category || "") !== ""
                height: visible ? categoryLayout.height + divider.height : 0

                ListItemLayout {
                    id: categoryLayout
                    title.text: i18n.tr("Category")
                    subtitle.text: infoPage.info.category || ""
                }
            }

            ListItem {
                height: idLayout.height + divider.height

                trailingActions: ListItemActions {
                    actions: [
                        Action {
                            iconName: "edit-copy"
                            text: i18n.tr("Copy")
                            onTriggered: Clipboard.push(infoPage.info.id || "")
                        }
                    ]
                }

                ListItemLayout {
                    id: idLayout
                    title.text: i18n.tr("ID")
                    subtitle.text: infoPage.info.id || ""
                }
            }

            ListItem {
                visible: infoPage.members.length > 0
                height: visible ? membersHeader.height : 0
                divider.visible: false

                ListItemLayout {
                    id: membersHeader
                    title.text: i18n.tr("Members (%1)").arg(infoPage.members.length)
                    title.font.bold: true
                }
            }

            Repeater {
                model: infoPage.members

                delegate: ListItem {
                    required property var modelData

                    height: memberLayout.height + divider.height

                    ListItemLayout {
                        id: memberLayout
                        title.text: modelData.name
                        title.font.strikeout: modelData.blocked
                        title.color: modelData.blocked ? theme.palette.normal.backgroundSecondaryText
                                                       : theme.palette.normal.backgroundText
                        subtitle.text: modelData.blocked ? i18n.tr("Blocked") + " · " + modelData.username
                                                         : modelData.username

                        SidebarIcon {
                            SlotsLayout.position: SlotsLayout.Leading
                            width: units.gu(4.5)
                            height: width
                            imageSource: modelData.avatarUrl
                            label: modelData.name.charAt(0)
                        }

                        Icon {
                            SlotsLayout.position: SlotsLayout.Trailing
                            visible: modelData.blocked
                            width: units.gu(2)
                            height: width
                            name: "cancel"
                            color: theme.palette.normal.negative
                        }
                    }
                }
            }
        }
    }
}
