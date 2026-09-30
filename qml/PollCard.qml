import QtQuick
import Lomiri.Components
import Disports.Core

// A poll, in Lomiri's list style: the question as a header, then one row per
// answer with a tick (single choice, like OptionSelector) or a CheckBox
// (multiple choice). Tap an answer to vote, again to take the vote back.
// Once you voted or the poll closed, each answer shows its votes and a
// ProgressBar.
LomiriShape {
    id: card

    property string messageId: ""
    // See MessageListModel::pollOf.
    property var poll: ({})

    readonly property var answers: poll.answers || []
    readonly property var myVotes: answers.filter(function(a) { return a.me }).map(function(a) { return a.id })
    readonly property bool showResults: poll.closed || myVotes.length > 0

    height: column.height
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
        width: parent.width

        ListItemLayout {
            id: header
            title.text: card.poll.question || ""
            title.font.bold: true
            title.wrapMode: Text.Wrap
            title.maximumLineCount: 4
            subtitle.text: {
                const votes = card.poll.totalVotes || 0
                const parts = [card.poll.closed ? i18n.tr("Poll closed") : i18n.tr("Poll")]
                parts.push(i18n.tr("%1 vote", "%1 votes", votes).arg(votes))
                if (card.poll.expires)
                    parts.push(card.poll.expires)
                if (!card.poll.closed && card.poll.multiselect)
                    parts.push(i18n.tr("pick any"))
                return parts.join(" · ")
            }
        }

        Repeater {
            model: card.answers

            delegate: AbstractButton {
                id: answer

                required property var modelData

                width: column.width
                height: layout.height + (bar.visible ? bar.height + units.gu(1) : 0) + divider.height
                // Timeouts take voting away too.
                enabled: !card.poll.closed && Session.timeoutText === ""
                onClicked: card.vote(modelData.id)

                // Pressed feedback, like a ListItem
                Rectangle {
                    anchors.fill: parent
                    visible: answer.pressed
                    color: theme.palette.highlighted.background
                }

                Rectangle {
                    id: divider
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: units.dp(1)
                    color: theme.palette.normal.base
                }

                ListItemLayout {
                    id: layout
                    anchors.top: divider.bottom
                    title.text: (answer.modelData.emoji ? answer.modelData.emoji + "  " : "") + answer.modelData.text
                    title.font.bold: answer.modelData.me
                    summary.text: card.showResults
                                  ? i18n.tr("%1 vote", "%1 votes", answer.modelData.votes).arg(answer.modelData.votes)
                                    + " · " + answer.modelData.percent + "%"
                                  : ""

                    Image {
                        SlotsLayout.position: SlotsLayout.Leading
                        visible: answer.modelData.emojiUrl !== ""
                        width: units.gu(2.5)
                        height: width
                        source: answer.modelData.emojiUrl
                        sourceSize.width: units.gu(5)
                        asynchronous: true
                    }

                    // Multiple choice
                    CheckBox {
                        SlotsLayout.position: SlotsLayout.Trailing
                        visible: card.poll.multiselect
                        checked: answer.modelData.me
                        enabled: !card.poll.closed && Session.timeoutText === ""
                        onTriggered: {
                            checked = Qt.binding(function() { return answer.modelData.me })
                            card.vote(answer.modelData.id)
                        }
                    }

                    // Single choice: the tick of an OptionSelector
                    Icon {
                        SlotsLayout.position: SlotsLayout.Trailing
                        visible: !card.poll.multiselect
                        width: units.gu(2)
                        height: width
                        name: "tick"
                        opacity: answer.modelData.me ? 1 : 0
                        color: theme.palette.normal.activity
                    }
                }

                ProgressBar {
                    id: bar
                    visible: card.showResults
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: layout.bottom
                        leftMargin: units.gu(2)
                        rightMargin: units.gu(2)
                    }
                    minimumValue: 0
                    maximumValue: 100
                    value: answer.modelData.percent
                    showProgressPercentage: false
                }
            }
        }
    }
}
