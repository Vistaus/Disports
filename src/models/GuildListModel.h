#pragma once

class VoiceStates;

#include <QAbstractListModel>
#include <QSet>

#include <vector>

#include "discord/models/Snowflake.hpp"

// The server rail: servers and server folders in the user's order. A folder
// row is followed by its servers while it is expanded.
class GuildListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int directMessageMentions READ directMessageMentions NOTIFY unreadChanged)

public:
    enum Roles {
        KindRole = Qt::UserRole + 1, // "guild" or "folder"
        ItemIdRole,                  // guild or folder id
        NameRole,
        IconUrlRole,
        InitialsRole,
        UnreadRole,
        MentionsRole,
        FolderIdRole,                // folder the guild is in, or ""
        FolderColorRole,             // "#rrggbb", or "" for no colour
        ExpandedRole,                // folders only
        PreviewsRole,                // folders only: up to 4 {iconUrl, initials}
        GuildIdsRole,                // folders only: ids of the servers inside
        HasCallRole,                 // someone is in a voice channel (in the folder)
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Rebuilds the list from the core (READY, guild or folder changes).
    void reload();
    // Unread markers changed; the rows stay the same.
    void refreshUnread();
    void clear();

    Q_INVOKABLE void toggleFolder(const QString& folderId);

    int directMessageMentions() const;

    void setVoiceStates(const VoiceStates* states) { m_voiceStates = states; }
    // Who is in voice changed.
    void refreshCalls();

signals:
    void countChanged();
    void unreadChanged();

private:
    struct Row {
        bool folder = false;
        Snowflake id = 0;
        Snowflake folderId = 0;
        int color = -1;
        QString name;
        std::vector<Snowflake> guilds; // folders only
    };

    std::vector<Row> buildRows() const;

    std::vector<Row> m_rows;
    QSet<Snowflake> m_expanded;
    const VoiceStates* m_voiceStates = nullptr;
};
