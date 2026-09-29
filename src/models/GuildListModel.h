#pragma once

#include <QAbstractListModel>

#include <vector>

#include "discord/models/Snowflake.hpp"

// Servers in the sidebar, in the user's order (folders flattened).
class GuildListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int directMessageMentions READ directMessageMentions NOTIFY unreadChanged)

public:
    enum Roles {
        GuildIdRole = Qt::UserRole + 1,
        NameRole,
        IconUrlRole,
        InitialsRole,
        UnreadRole,
        MentionsRole,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Rebuilds the list from the core (READY, guild create/delete).
    void reload();
    // Unread markers changed; the rows stay the same.
    void refreshUnread();
    void clear();

    int directMessageMentions() const;

signals:
    void countChanged();
    void unreadChanged();

private:
    std::vector<Snowflake> m_ids;
};
