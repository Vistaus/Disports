#include "ChannelPermissions.h"

#include <QDateTime>
#include <QLocale>

#include <ctime>

#include "discord/DiscordInstance.hpp"
#include "discord/models/Permissions.hpp"

#include "Session.h"

ChannelPermissions::ChannelPermissions(Session* session)
    : QObject(session)
    , m_session(session)
{
    m_timeoutEnd.setSingleShot(true);
    connect(&m_timeoutEnd, &QTimer::timeout, this, &ChannelPermissions::update);
}

Channel* ChannelPermissions::channel() const
{
    DiscordInstance* instance = m_session->instance();
    return instance ? instance->GetCurrentChannel() : nullptr;
}

Channel* ChannelPermissions::serverChannel() const
{
    Channel* current = channel();
    return current && !current->IsDM() ? current : nullptr;
}

bool ChannelPermissions::has(uint64_t permission) const
{
    Channel* current = channel();
    return current && (current->IsDM() || current->HasPermission(permission));
}

bool ChannelPermissions::canSendMessages() const
{
    return has(PERM_SEND_MESSAGES);
}

bool ChannelPermissions::canManageMessages() const
{
    Channel* current = serverChannel();
    return current && current->HasPermission(PERM_MANAGE_MESSAGES);
}

bool ChannelPermissions::canAddReactions() const
{
    return has(PERM_ADD_REACTIONS) && has(PERM_READ_MESSAGE_HISTORY);
}

bool ChannelPermissions::canUseReactions() const
{
    // A timeout takes reactions away too.
    return has(PERM_READ_MESSAGE_HISTORY) && timeoutEndMs() == 0;
}

bool ChannelPermissions::canReadHistory() const
{
    return has(PERM_READ_MESSAGE_HISTORY);
}

bool ChannelPermissions::canAttachFiles() const
{
    return canSendMessages() && has(PERM_ATTACH_FILES);
}

bool ChannelPermissions::canMentionEveryone() const
{
    return serverChannel() && has(PERM_MENTION_EVERYONE);
}

qint64 ChannelPermissions::timeoutEndMs() const
{
    Channel* current = serverChannel();
    Profile* me = current ? m_session->instance()->GetProfile() : nullptr;
    if (!me)
        return 0;
    auto member = me->m_guildMembers.find(current->m_parentGuild);
    if (member == me->m_guildMembers.end() || member->second.m_timeoutUntil <= time(nullptr))
        return 0;
    // Administrators can't be timed out.
    if (current->HasPermission(PERM_ADMINISTRATOR))
        return 0;
    return qint64(member->second.m_timeoutUntil) * 1000;
}

QString ChannelPermissions::timeoutText() const
{
    const qint64 end = timeoutEndMs();
    if (!end)
        return QString();
    const QDateTime until = QDateTime::fromMSecsSinceEpoch(end);
    const QString when = until.date() == QDate::currentDate()
        ? QLocale().toString(until.time(), QLocale::ShortFormat)
        : QLocale().toString(until, QLocale::ShortFormat);
    return tr("You're timed out until %1").arg(when);
}

int ChannelPermissions::slowmodeSeconds() const
{
    Channel* current = serverChannel();
    if (!current || current->HasPermission(PERM_MANAGE_MESSAGES) || current->HasPermission(PERM_MANAGE_CHANNELS))
        return 0;
    return current->m_slowmodeSeconds;
}

void ChannelPermissions::update()
{
    // Timeouts end by themselves: look again then.
    if (const qint64 end = timeoutEndMs()) {
        const qint64 wait = end - QDateTime::currentMSecsSinceEpoch() + 500;
        m_timeoutEnd.start(int(qBound(qint64(1000), wait, qint64(3600000))));
    } else {
        m_timeoutEnd.stop();
    }
    emit changed();
}
