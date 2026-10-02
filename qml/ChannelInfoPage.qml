import QtQuick
import Lomiri.Components
import Disports.Core

// About a channel, DM or group: its type, server and topic; for a DM the
// other person (status, about me); for a group who is in it.
Page {
    id: infoPage
    objectName: "channelInfoPage"

    property string channelId: ""
    property var info: ({})
    readonly property var members: info.members || []
    // A 1:1 DM: the other person.
    readonly property var user: info.user || null

    function refresh() { info = Session.channelInfo(channelId) }
    Component.onCompleted: refresh()
    onChannelIdChanged: refresh()

    // Their full profile (about me, pronouns) arriving, and status changes.
    Connections {
        target: Session
        function onProfileChanged() { infoPage.refresh() }
    }

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

            // A 1:1 DM: the other person's profile.
            ProfileView {
                width: parent.width
                visible: !!infoPage.user
                height: visible ? implicitHeight : 0
                user: infoPage.user || ({})
            }

            // Channels and groups: name and kind.
            ListItem {
                visible: !infoPage.user
                height: visible ? titleLayout.height + divider.height : 0

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

            // A DM's profile has the person's own ID.
            ListItem {
                visible: !infoPage.user
                height: visible ? idLayout.height + divider.height : 0

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
                    onClicked: infoPage.pageStack.push(Qt.resolvedUrl("ProfilePage.qml"), { "userId": modelData.id })

                    ListItemLayout {
                        id: memberLayout
                        title.text: modelData.name
                        title.font.strikeout: modelData.blocked
                        title.color: modelData.blocked ? theme.palette.normal.backgroundSecondaryText
                                                       : theme.palette.normal.backgroundText
                        subtitle.text: (modelData.self ? i18n.tr("You") + " · " : "")
                                       + (modelData.blocked ? i18n.tr("Blocked") + " · " : "") + modelData.username

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
