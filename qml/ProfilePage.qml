import QtQuick
import Lomiri.Components
import Disports.Core

// Someone's profile, opened from their picture or name (messages, members,
// calls).
Page {
    id: profilePage
    objectName: "profilePage"

    property string userId: ""
    property var user: ({})

    function refresh() { user = Session.userInfo(userId) }
    Component.onCompleted: refresh()
    onUserIdChanged: refresh()

    // Their full profile (about me, pronouns) arriving, and status changes.
    Connections {
        target: Session
        function onProfileChanged() { profilePage.refresh() }
    }

    header: PageHeader {
        title: profilePage.user.name || ""
    }

    Flickable {
        anchors {
            top: profilePage.header.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        contentHeight: profile.height
        clip: true

        ProfileView {
            id: profile
            width: parent.width
            user: profilePage.user
            directMessageId: Session.directMessageWith(profilePage.userId)
            // Back to the main page, into the conversation.
            onMessageRequested: function(channelId) {
                const stack = profilePage.pageStack
                while (stack.depth > 1)
                    stack.pop()
                stack.currentPage.openChannel(channelId)
            }
        }
    }
}
