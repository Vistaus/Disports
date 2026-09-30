#include "TypingIndicator.h"

#include <QDateTime>
#include <QStringList>

#include "discord/DiscordInstance.hpp"

#include "MessageFormatter.h"
#include "Session.h"

namespace {

constexpr qint64 TypingDurationMs = 10000;

}

TypingIndicator::TypingIndicator(Session* session)
    : QObject(session)
    , m_session(session)
{
    m_expiry.setInterval(1000);
    connect(&m_expiry, &QTimer::timeout, this, &TypingIndicator::refresh);
}

void TypingIndicator::notifyTyping()
{
    if (m_session->instance() && m_session->connected())
        m_session->instance()->Typing();
}

void TypingIndicator::userTyping(Snowflake user, Snowflake channel)
{
    DiscordInstance* instance = m_session->instance();
    if (!instance || channel != instance->GetCurrentChannelID() || user == instance->GetUserID())
        return;
    m_typingUntil.insert(user, QDateTime::currentMSecsSinceEpoch() + TypingDurationMs);
    refresh();
}

void TypingIndicator::userSent(Snowflake user)
{
    if (m_typingUntil.remove(user))
        refresh();
}

void TypingIndicator::clear()
{
    m_typingUntil.clear();
    refresh();
}

void TypingIndicator::refresh()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_typingUntil.begin(); it != m_typingUntil.end();)
        it = it.value() <= now ? m_typingUntil.erase(it) : std::next(it);

    QStringList names;
    if (DiscordInstance* instance = m_session->instance()) {
        const Snowflake guild = instance->GetCurrentGuildID();
        for (auto it = m_typingUntil.constBegin(); it != m_typingUntil.constEnd(); ++it)
            names.append(MessageFormatter::displayName(it.key(), guild, tr("Someone")));
    }
    names.sort();

    QString text;
    if (names.size() == 1)
        text = tr("%1 is typing...").arg(names[0]);
    else if (names.size() == 2)
        text = tr("%1 and %2 are typing...").arg(names[0], names[1]);
    else if (names.size() > 2)
        text = tr("Several people are typing...");

    if (names.isEmpty())
        m_expiry.stop();
    else if (!m_expiry.isActive())
        m_expiry.start();

    if (text != m_text) {
        m_text = text;
        emit textChanged();
    }
}
