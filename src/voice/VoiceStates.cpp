#include "VoiceStates.h"

#include <QTimer>

#include <set>

#include "discord/DiscordInstance.hpp"
#include "discord/state/ProfileCache.hpp"

namespace {

Snowflake snowflakeOf(const nlohmann::json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key))
        return 0;
    const nlohmann::json& value = object[key];
    if (value.is_string())
        return Snowflake(std::stoull(value.get<std::string>()));
    if (value.is_number_unsigned())
        return Snowflake(value.get<uint64_t>());
    return 0;
}

// Discord's epoch (2015), and the milliseconds in a snowflake.
qint64 snowflakeTimeMs(Snowflake id)
{
    return qint64(uint64_t(id) >> 22) + 1420070400000ll;
}

}

bool VoiceStates::hasCall(Snowflake channel) const
{
    return m_dmCalls.contains(channel) || !m_channelUsers.value(channel).isEmpty();
}

bool VoiceStates::guildHasCall(Snowflake guild) const
{
    for (auto it = m_channelGuild.cbegin(); it != m_channelGuild.cend(); ++it) {
        if (it.value() == guild && !m_channelUsers.value(it.key()).isEmpty())
            return true;
    }
    return false;
}

qint64 VoiceStates::callStartedMs(Snowflake channel) const
{
    const Snowflake message = m_dmCalls.value(channel);
    return message ? snowflakeTimeMs(message) : 0;
}

void VoiceStates::clear()
{
    m_userChannel.clear();
    m_channelUsers.clear();
    m_channelGuild.clear();
    m_dmCalls.clear();
    emit changed(0, 0);
}

void VoiceStates::setUserChannel(Snowflake guild, Snowflake user, Snowflake channel, bool notify)
{
    const QPair<Snowflake, Snowflake> key(guild, user);
    const Snowflake previous = m_userChannel.value(key);
    if (previous == channel)
        return;
    if (previous) {
        QList<Snowflake>& users = m_channelUsers[previous];
        users.removeAll(user);
        if (users.isEmpty())
            m_channelUsers.remove(previous);
        if (notify)
            emit changed(guild, previous);
    }
    if (channel) {
        m_userChannel.insert(key, channel);
        m_channelUsers[channel].append(user);
        m_channelGuild.insert(channel, guild);
        if (notify)
            emit changed(guild, channel);
    } else {
        m_userChannel.remove(key);
    }
}

void VoiceStates::loadGuildStates(Snowflake guild, const nlohmann::json& states)
{
    if (!states.is_array())
        return;
    QList<Snowflake> users;
    for (const nlohmann::json& state : states) {
        const Snowflake user = snowflakeOf(state, "user_id");
        const Snowflake channel = snowflakeOf(state, "channel_id");
        if (!user || !channel)
            continue;
        setUserChannel(guild, user, channel, false);
        users.append(user);
    }
    requestUnknownMembers(guild, users);
}

void VoiceStates::requestUnknownMembers(Snowflake guild, const QList<Snowflake>& users)
{
    if (users.isEmpty() || !guild)
        return;
    // After the core has handled this event (the server is loaded by then).
    QTimer::singleShot(0, this, [guild, users]() {
        DiscordInstance* instance = GetDiscordInstance();
        if (!instance)
            return;
        std::set<Snowflake> unknown;
        for (Snowflake user : users) {
            Profile* profile = GetProfileCache()->LookupProfile(user, "", "", "", false);
            if (!profile || profile->GetUsername().empty())
                unknown.insert(user);
        }
        if (!unknown.empty())
            instance->RequestGuildMembers(guild, unknown);
    });
}

void VoiceStates::gatewayDispatch(const std::string& type, const nlohmann::json& message)
{
    if (!message.contains("d"))
        return;
    const nlohmann::json& d = message["d"];

    if (type == "READY") {
        m_userChannel.clear();
        m_channelUsers.clear();
        m_channelGuild.clear();
        m_dmCalls.clear();
        if (d.contains("guilds") && d["guilds"].is_array()) {
            for (const nlohmann::json& guild : d["guilds"]) {
                if (guild.contains("voice_states"))
                    loadGuildStates(snowflakeOf(guild, "id"), guild["voice_states"]);
            }
        }
        emit changed(0, 0);
    } else if (type == "GUILD_CREATE") {
        const Snowflake guild = snowflakeOf(d, "id");
        if (d.contains("voice_states"))
            loadGuildStates(guild, d["voice_states"]);
        emit changed(guild, 0);
    } else if (type == "VOICE_STATE_UPDATE") {
        const Snowflake guild = snowflakeOf(d, "guild_id");
        const Snowflake user = snowflakeOf(d, "user_id");
        if (!user)
            return;
        // The member, with name and picture, comes along in servers.
        if (guild && d.contains("member") && d["member"].is_object()) {
            if (DiscordInstance* instance = GetDiscordInstance()) {
                nlohmann::json member = d["member"];
                instance->ParseGuildMember(guild, member, user);
            }
        }
        setUserChannel(guild, user, snowflakeOf(d, "channel_id"));
    } else if (type == "CALL_CREATE" || type == "CALL_UPDATE") {
        const Snowflake channel = snowflakeOf(d, "channel_id");
        if (!channel)
            return;
        if (const Snowflake call = snowflakeOf(d, "message_id"))
            m_dmCalls.insert(channel, call);
        else if (!m_dmCalls.contains(channel))
            m_dmCalls.insert(channel, 0);
        if (d.contains("voice_states") && d["voice_states"].is_array()) {
            for (const nlohmann::json& state : d["voice_states"])
                setUserChannel(0, snowflakeOf(state, "user_id"), channel, false);
        }
        emit changed(0, channel);
    } else if (type == "CALL_DELETE") {
        const Snowflake channel = snowflakeOf(d, "channel_id");
        m_dmCalls.remove(channel);
        for (Snowflake user : m_channelUsers.value(channel))
            m_userChannel.remove(qMakePair(Snowflake(0), user));
        m_channelUsers.remove(channel);
        emit changed(0, channel);
    }
}
