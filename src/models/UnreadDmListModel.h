#pragma once

#include <QAbstractListModel>

#include <vector>

#include "discord/models/Snowflake.hpp"

// Direct message conversations with unread messages, shown under the
// Direct Messages button in the server rail.
class UnreadDmListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        ChannelIdRole = Qt::UserRole + 1,
        NameRole,
        IconUrlRole,
        InitialsRole,
        MentionsRole,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void reload();
    void clear();

signals:
    void countChanged();

private:
    std::vector<Snowflake> m_ids;
};
