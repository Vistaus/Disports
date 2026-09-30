#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>

class Channel;
class Session;

// What the account may do in the open channel: server permissions with
// role and member overwrites, and timeouts. Direct messages and groups
// allow everything.
class ChannelPermissions : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canSendMessages READ canSendMessages NOTIFY changed)
    // Delete other people's messages.
    Q_PROPERTY(bool canManageMessages READ canManageMessages NOTIFY changed)
    // Add a new reaction, or join in on an existing one (which Discord
    // allows without "Add Reactions").
    Q_PROPERTY(bool canAddReactions READ canAddReactions NOTIFY changed)
    Q_PROPERTY(bool canUseReactions READ canUseReactions NOTIFY changed)
    Q_PROPERTY(bool canReadHistory READ canReadHistory NOTIFY changed)
    Q_PROPERTY(bool canAttachFiles READ canAttachFiles NOTIFY changed)
    // @everyone, @here and roles that aren't mentionable.
    Q_PROPERTY(bool canMentionEveryone READ canMentionEveryone NOTIFY changed)
    // "You're timed out until 14:30", or empty.
    Q_PROPERTY(QString timeoutText READ timeoutText NOTIFY changed)
    // Seconds between messages, 0 without slowmode or for moderators.
    Q_PROPERTY(int slowmodeSeconds READ slowmodeSeconds NOTIFY changed)

public:
    explicit ChannelPermissions(Session* session);

    bool canSendMessages() const;
    bool canManageMessages() const;
    bool canAddReactions() const;
    bool canUseReactions() const;
    bool canReadHistory() const;
    bool canAttachFiles() const;
    bool canMentionEveryone() const;
    QString timeoutText() const;
    int slowmodeSeconds() const;

    // After switching channels, and when roles, members or overwrites change.
    void update();

signals:
    void changed();

private:
    Channel* channel() const;
    // Null in direct messages and groups.
    Channel* serverChannel() const;
    bool has(uint64_t permission) const;
    qint64 timeoutEndMs() const;

    Session* m_session;
    QTimer m_timeoutEnd;
};
