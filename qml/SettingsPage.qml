import QtQuick
import Lomiri.Components
import Lomiri.Components.Popups as Popups
import Disports.Core

Page {
    id: settingsPage

    // A collapsed OptionSelector shows its first option until it has been
    // opened and closed once (Lomiri only scrolls its list to the selected
    // one after collapsing): scroll it there ourselves.
    function showSelected(selector) {
        if (selector.currentlyExpanded)
            return
        function listIn(item) {
            for (let i = 0; i < item.children.length; ++i) {
                const child = item.children[i]
                if (child.positionViewAtIndex !== undefined && child.itemHeight !== undefined)
                    return child
                const found = listIn(child)
                if (found)
                    return found
            }
            return null
        }
        const list = listIn(selector)
        if (list)
            Qt.callLater(function() { list.positionViewAtIndex(selector.selectedIndex, ListView.Beginning) })
    }

    header: PageHeader {
        title: i18n.tr("Settings")
    }

    Flickable {
        anchors {
            top: settingsPage.header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        contentHeight: settingsColumn.height
        clip: true

        Column {
            id: settingsColumn
            width: parent.width

            ListItem {
                height: accountLayout.height + (divider.visible ? divider.height : 0)

                ListItemLayout {
                    id: accountLayout
                    title.text: Session.username
                    title.font.bold: true

                    SidebarIcon {
                        SlotsLayout.position: SlotsLayout.Leading
                        width: units.gu(5)
                        height: width
                        imageSource: Session.avatarUrl
                    }
                }
            }

            ListItem {
                height: appearanceHeader.height
                divider.visible: false
                ListItemLayout {
                    id: appearanceHeader
                    title.text: i18n.tr("Theme")
                    title.font.bold: true
                }
            }

            Item {
                width: parent.width
                height: themeSelector.height + units.gu(2)

                OptionSelector {
                    id: themeSelector
                    anchors { left: parent.left; right: parent.right; margins: units.gu(2) }
                    expanded: false
                    // Same order as Preferences.themeMode: 0 light, 1 dark, 2 system.
                    model: [i18n.tr("Light"), i18n.tr("Dark"), i18n.tr("Follow system")]
                    onDelegateClicked: function(index) { Session.preferences.themeMode = index }
                    onSelectedIndexChanged: settingsPage.showSelected(themeSelector)
                }

                // selectedIndex is the list's currentIndex, which the model
                // resets to 0 when set after it: apply it once built.
                Binding {
                    target: themeSelector
                    property: "selectedIndex"
                    value: Session.preferences.themeMode
                    delayed: true
                }
            }

            ListItem {
                height: dmHeader.height
                divider.visible: false
                ListItemLayout {
                    id: dmHeader
                    title.text: i18n.tr("Direct messages")
                    title.font.bold: true
                }
            }

            ListItem {
                height: picturesLayout.height + (divider.visible ? divider.height : 0)
                onClicked: picturesSwitch.trigger()

                ListItemLayout {
                    id: picturesLayout
                    title.text: i18n.tr("Show profile pictures")
                    summary.text: i18n.tr("Off: icons and status only, which loads fewer images")
                    summary.wrapMode: Text.WordWrap
                    summary.maximumLineCount: 3

                    Switch {
                        id: picturesSwitch
                        SlotsLayout.position: SlotsLayout.Trailing
                        checked: Session.preferences.dmProfilePictures
                        onTriggered: Session.preferences.dmProfilePictures = checked
                    }
                }
            }

            ListItem {
                height: chatHeader.height
                divider.visible: false
                ListItemLayout {
                    id: chatHeader
                    title.text: i18n.tr("Chat")
                    title.font.bold: true
                }
            }

            ListItem {
                height: chatPicturesLayout.height + (divider.visible ? divider.height : 0)
                onClicked: chatPicturesSwitch.trigger()

                ListItemLayout {
                    id: chatPicturesLayout
                    title.text: i18n.tr("Show profile pictures")
                    summary.text: i18n.tr("Off: messages use the full width")
                    summary.wrapMode: Text.WordWrap
                    summary.maximumLineCount: 3

                    Switch {
                        id: chatPicturesSwitch
                        SlotsLayout.position: SlotsLayout.Trailing
                        checked: Session.preferences.chatProfilePictures
                        onTriggered: Session.preferences.chatProfilePictures = checked
                    }
                }
            }

            ListItem {
                height: linesLayout.height + linesSlider.height + units.gu(1)

                ListItemLayout {
                    id: linesLayout
                    title.text: i18n.tr("Message box lines")
                    summary.text: i18n.tr("How far the message box grows before it scrolls")

                    Label {
                        SlotsLayout.position: SlotsLayout.Trailing
                        text: Session.preferences.composerMaxLines
                        font.bold: true
                    }
                }

                Slider {
                    id: linesSlider
                    anchors { left: parent.left; right: parent.right; top: linesLayout.bottom; leftMargin: units.gu(2); rightMargin: units.gu(2) }
                    minimumValue: 1
                    maximumValue: 6
                    stepSize: 1
                    live: true
                    value: Session.preferences.composerMaxLines
                    function formatValue(v) { return Math.round(v) }
                    onValueChanged: Session.preferences.composerMaxLines = Math.round(value)
                }
            }

            ListItem {
                height: blockedHeader.height
                divider.visible: false
                ListItemLayout {
                    id: blockedHeader
                    title.text: i18n.tr("Messages from blocked users")
                    subtitle.text: ""
                }
            }

            Item {
                width: parent.width
                height: blockedSelector.height + units.gu(2)

                OptionSelector {
                    id: blockedSelector
                    readonly property var modes: ["hide", "reveal", "show"]
                    anchors { left: parent.left; right: parent.right; margins: units.gu(2) }
                    expanded: false
                    model: [i18n.tr("Hide completely"), i18n.tr("Show a placeholder to tap"), i18n.tr("Show normally")]
                    onDelegateClicked: function(index) { Session.preferences.blockedMessages = modes[index] }
                    onSelectedIndexChanged: settingsPage.showSelected(blockedSelector)
                }

                Binding {
                    target: blockedSelector
                    property: "selectedIndex"
                    value: Math.max(0, blockedSelector.modes.indexOf(Session.preferences.blockedMessages))
                    delayed: true
                }
            }

            ListItem {
                height: gifLayout.height + (divider.visible ? divider.height : 0)
                onClicked: gifSwitch.trigger()

                ListItemLayout {
                    id: gifLayout
                    title.text: i18n.tr("Play GIFs in the chat")
                    summary.text: i18n.tr("Off: GIFs play when you open them, which saves data and battery")
                    summary.wrapMode: Text.WordWrap
                    summary.maximumLineCount: 3

                    Switch {
                        id: gifSwitch
                        SlotsLayout.position: SlotsLayout.Trailing
                        checked: Session.preferences.autoplayGifs
                        onTriggered: Session.preferences.autoplayGifs = checked
                    }
                }
            }

            ListItem {
                height: callsHeader.height
                divider.visible: false
                ListItemLayout {
                    id: callsHeader
                    title.text: i18n.tr("Calls")
                    title.font.bold: true
                }
            }

            ListItem {
                height: processingLayout.height + (divider.visible ? divider.height : 0)
                onClicked: processingSwitch.trigger()

                ListItemLayout {
                    id: processingLayout
                    title.text: i18n.tr("Echo cancellation")
                    summary.text: i18n.tr("Keeps others from hearing themselves through your loudspeaker, and evens out your volume. Applies from the next call")
                    summary.wrapMode: Text.WordWrap
                    summary.maximumLineCount: 4

                    Switch {
                        id: processingSwitch
                        SlotsLayout.position: SlotsLayout.Trailing
                        checked: Session.preferences.voiceProcessing
                        onTriggered: Session.preferences.voiceProcessing = checked
                    }
                }
            }

            ListItem {
                height: denoiseLayout.height + (divider.visible ? divider.height : 0)
                onClicked: denoiseSwitch.trigger()

                ListItemLayout {
                    id: denoiseLayout
                    title.text: i18n.tr("Noise suppression")
                    summary.text: i18n.tr("Removes background noise such as typing, fans and traffic from your microphone. Applies from the next call")
                    summary.wrapMode: Text.WordWrap
                    summary.maximumLineCount: 4

                    Switch {
                        id: denoiseSwitch
                        SlotsLayout.position: SlotsLayout.Trailing
                        checked: Session.preferences.noiseSuppression
                        onTriggered: Session.preferences.noiseSuppression = checked
                    }
                }
            }

            Item {
                width: parent.width
                height: units.gu(2)
            }

            Button {
                anchors { left: parent.left; right: parent.right; margins: units.gu(2) }
                text: i18n.tr("Log out")
                color: theme.palette.normal.negative
                onClicked: Popups.PopupUtils.open(logoutDialog)
            }

            Label {
                anchors { left: parent.left; right: parent.right; margins: units.gu(2) }
                topPadding: units.gu(2)
                bottomPadding: units.gu(2)
                text: i18n.tr("Disports %1").arg(Qt.application.version)
                wrapMode: Text.WordWrap
                textSize: Label.Small
                color: theme.palette.normal.backgroundSecondaryText
            }
        }
    }

    Component {
        id: logoutDialog

        Popups.Dialog {
            id: dialog
            title: i18n.tr("Log out?")
            text: i18n.tr("You'll need to sign in again to use Disports.")

            Button {
                text: i18n.tr("Cancel")
                onClicked: Popups.PopupUtils.close(dialog)
            }

            Button {
                text: i18n.tr("Log out")
                color: theme.palette.normal.negative
                onClicked: {
                    Popups.PopupUtils.close(dialog)
                    Session.logout()
                }
            }
        }
    }
}
