#pragma once

#include <QAbstractListModel>

#include <vector>

#include "discord/models/Snowflake.hpp"

struct Channel;

// Channels of the selected server (grouped by category), or the direct
// message list when "Direct Messages" is selected.
class ChannelListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        ChannelIdRole = Qt::UserRole + 1,
        NameRole,
        KindRole,        // "text", "announcement", "voice", "forum", "category", "dm", "group"
        IsCategoryRole,
        OpenableRole,
        UnreadRole,
        MentionsRole,
        IconUrlRole,     // direct messages only
        TopicRole,
        IndentedRole,    // belongs to a category
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Rebuilds the list for the core's current guild.
    void reload();
    void refreshChannel(Snowflake channel);
    static QString displayName(const Channel& channel);
    void refreshAll();
    void clear();

signals:
    void countChanged();

private:
    Snowflake m_guild = 0;
    std::vector<Snowflake> m_ids;
};
