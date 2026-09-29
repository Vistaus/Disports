import QtQuick
import Lomiri.Components
import Disports.Core

// A poll: tap an answer to vote, tap it again to take the vote back. Results
// show once you voted or the poll closed, like Discord.
LomiriShape {
    id: card

    property string messageId: ""
    // See MessageListModel::pollOf.
    property var poll: ({})

    readonly property var answers: poll.answers || []
    readonly property var myVotes: answers.filter(function(a) { return a.me }).map(function(a) { return a.id })
    readonly property bool showResults: poll.closed || myVotes.length > 0
    readonly property real padding: units.gu(1.25)

    height: column.height + padding * 2
    aspect: LomiriShape.Flat
    radius: "small"
    backgroundColor: theme.palette.normal.foreground

    function vote(answerId) {
        if (poll.closed)
            return
        let votes
        if (myVotes.indexOf(answerId) >= 0)
            votes = myVotes.filter(function(id) { return id !== answerId })
        else
            votes = poll.multiselect ? myVotes.concat([answerId]) : [answerId]
        Session.votePoll(messageId, votes)
    }

    Column {
        id: column
        x: card.padding
        y: card.padding
        width: card.width - card.padding * 2
        spacing: units.gu(0.75)

        Label {
            width: parent.width
            text: card.poll.question || ""
            font.bold: true
            wrapMode: Text.Wrap
        }

        Label {
            width: parent.width
            text: card.poll.closed ? i18n.tr("Poll closed")
                  : card.poll.multiselect ? i18n.tr("Select one or more answers")
                  : i18n.tr("Select one answer")
            textSize: Label.XSmall
            color: theme.palette.normal.backgroundSecondaryText
        }

        Repeater {
            model: card.answers

            delegate: AbstractButton {
                id: answer

                required property var modelData

                width: column.width
                height: units.gu(4.5)
                enabled: !card.poll.closed
                onClicked: card.vote(modelData.id)

                LomiriShape {
                    anchors.fill: parent
                    aspect: LomiriShape.Flat
                    radius: "small"
                    backgroundColor: theme.palette.normal.base
                }

                // Share of the votes
                LomiriShape {
                    visible: card.showResults && answer.modelData.percent > 0
                    anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                    width: parent.width * answer.modelData.percent / 100
                    aspect: LomiriShape.Flat
                    radius: "small"
                    backgroundColor: answer.modelData.me ? Qt.rgba(theme.palette.normal.focus.r, theme.palette.normal.focus.g,
                                                                   theme.palette.normal.focus.b, 0.35)
                                                         : Qt.rgba(theme.palette.normal.backgroundText.r,
                                                                   theme.palette.normal.backgroundText.g,
                                                                   theme.palette.normal.backgroundText.b, 0.12)
                }

                Row {
                    anchors {
                        left: parent.left
                        right: result.left
                        leftMargin: units.gu(1)
                        rightMargin: units.gu(1)
                        verticalCenter: parent.verticalCenter
                    }
                    spacing: units.gu(0.75)

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: text !== ""
                        text: answer.modelData.emoji
                    }

                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: answer.modelData.emojiUrl !== ""
                        width: units.gu(2.2)
                        height: width
                        source: answer.modelData.emojiUrl
                        sourceSize.width: units.gu(4)
                        asynchronous: true
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - x
                        text: answer.modelData.text
                        elide: Text.ElideRight
                        font.bold: answer.modelData.me
                    }
                }

                Row {
                    id: result
                    anchors { right: parent.right; rightMargin: units.gu(1); verticalCenter: parent.verticalCenter }
                    spacing: units.gu(0.75)

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: card.showResults
                        text: answer.modelData.percent + "%"
                        textSize: Label.Small
                        color: theme.palette.normal.backgroundSecondaryText
                    }

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !card.poll.closed || answer.modelData.me
                        width: units.gu(2)
                        height: width
                        name: answer.modelData.me ? "tick" : ""
                        color: theme.palette.normal.focus

                        // An empty circle or square to tap, like a radio
                        // button or a check box.
                        Rectangle {
                            anchors.fill: parent
                            visible: !answer.modelData.me
                            radius: card.poll.multiselect ? units.dp(3) : width / 2
                            color: "transparent"
                            border.width: units.dp(1.5)
                            border.color: theme.palette.normal.backgroundSecondaryText
                        }
                    }
                }
            }
        }

        Label {
            width: parent.width
            text: {
                const votes = card.poll.totalVotes || 0
                const parts = [i18n.tr("%1 vote", "%1 votes", votes).arg(votes)]
                if (card.poll.expires)
                    parts.push(card.poll.expires)
                return parts.join(" • ")
            }
            textSize: Label.XSmall
            color: theme.palette.normal.backgroundSecondaryText
        }
    }
}
