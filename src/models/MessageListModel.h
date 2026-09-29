#pragma once

#include <QAbstractListModel>
#include <QHash>

#include <memory>
#include <vector>

#include "discord/models/Message.hpp"
#include "discord/models/Snowflake.hpp"

// Messages of the open channel, newest first (the chat ListView runs
// bottom-to-top). Rows are read from Discord Messenger's MessageCache; gap
// markers become the hasOlder flag instead of rows.
class MessageListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool hasOlder READ hasOlder NOTIFY hasOlderChanged)
    Q_PROPERTY(bool reachedStart READ reachedStart NOTIFY hasOlderChanged)

public:
    enum Roles {
        MessageIdRole = Qt::UserRole + 1,
        AuthorIdRole,
        AuthorRole,
        AvatarUrlRole,
        BodyRole,          // rich text
        PlainBodyRole,
        TimestampRole,     // short, e.g. "Today at 14:02"
        EditedRole,
        IsOwnRole,
        IsPendingRole,
        IsSystemRole,
        GroupedRole,       // same author as the message above, shortly before
        HasReplyRole,
        ReplyAuthorRole,
        ReplyBodyRole,
        AttachmentsRole,   // list of {url, fileName, isImage, width, height}
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setChannel(Snowflake guild, Snowflake channel);
    Snowflake channel() const { return m_channel; }

    // Re-reads the cache and applies the difference to the rows.
    void sync();
    void clear();

    bool hasOlder() const { return m_olderGap != 0; }
    bool reachedStart() const { return m_reachedStart; }
    // The GAP_UP marker to load older messages from, or null.
    MessagePtr olderGap() const;
    // Newest real message, for read acknowledgement.
    Snowflake newestMessageId() const;

    void setOwnUserId(Snowflake user) { m_ownUser = user; }

signals:
    void countChanged();
    void hasOlderChanged();

private:
    struct Row {
        MessagePtr message;
        bool grouped = false;
    };

    std::vector<Row> readCache(Snowflake& olderGap, bool& reachedStart) const;
    static void computeGrouping(std::vector<Row>& rows);
    QString richBody(const Message& message) const;

    Snowflake m_guild = 0;
    Snowflake m_channel = 0;
    Snowflake m_ownUser = 0;
    Snowflake m_olderGap = 0;
    bool m_reachedStart = false;
    std::vector<Row> m_rows;
    mutable QHash<Snowflake, QString> m_bodyCache; // message id -> rich text
};
