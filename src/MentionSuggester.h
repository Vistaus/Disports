#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class Session;

// Suggestions for the word being typed in the open channel: "@al" finds
// people, roles and @everyone, "#gen" channels.
class MentionSuggester : public QObject
{
    Q_OBJECT

public:
    explicit MentionSuggester(Session* session);

    // Up to 8 of {kind ("user", "role", "everyone", "channel"), label,
    // detail, insert (the text that replaces the word), avatarUrl, color}.
    Q_INVOKABLE QVariantList suggestions(const QString& word) const;
    // Asks Discord for server members matching a name; they arrive later
    // (Session::membersChanged).
    Q_INVOKABLE void searchMembers(const QString& query);

private:
    Session* m_session;
};
