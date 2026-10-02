import QtQuick
import Lomiri.Components
import Lomiri.Components.Popups

// Shown once to people updating from Disports 0.8 (Session.updatedFromOldVersion,
// see src/Migration.h). Once they all have, this file and its use in
// Main.qml can go.
Dialog {
    id: notice

    title: i18n.tr("Welcome to Disports %1").arg(Qt.application.version)
    text: i18n.tr("Disports has been rebuilt from scratch for Ubuntu Touch 24.04. Your sign-in and settings were carried over from the old version.")

    Label {
        text: i18n.tr("New in this version:")
        font.bold: true
    }

    Label {
        width: parent.width
        wrapMode: Text.WordWrap
        text: [
            i18n.tr("Calls in DMs, groups and voice channels"),
            i18n.tr("A look more consistent with Ubuntu Touch"),
            i18n.tr("Recent conversations stay readable without a connection"),
            i18n.tr("Signing in with your email and password"),
        ].map(line => "• " + line).join("\n")
    }

    Label {
        width: parent.width
        wrapMode: Text.WordWrap
        textFormat: Text.StyledText
        linkColor: theme.palette.normal.activity
        text: i18n.tr("Something not working like before? <a href=\"%1\">Let us know on GitHub</a>.")
                  .arg("https://github.com/jukfiuune/Disports/issues")
        onLinkActivated: function(link) { Qt.openUrlExternally(link) }
    }

    Button {
        text: i18n.tr("Got it")
        color: theme.palette.normal.positive
        onClicked: PopupUtils.close(notice)
    }
}
