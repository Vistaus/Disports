#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>

#include <nlohmann/json.hpp>

#include "discord/models/Snowflake.hpp"

// Who is in which call, for the call markers in the lists: people in
// server voice channels (voice states from READY, GUILD_CREATE and
// VOICE_STATE_UPDATE) and calls in DMs and groups (CALL_CREATE, _UPDATE,
// _DELETE).
class VoiceStates : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // From the main gateway (see QtFrontend::OnGatewayDispatch); called
    // before the core handles the event.
    void gatewayDispatch(const std::string& type, const nlohmann::json& message);
    void clear();

    // People in a voice channel or DM call, in the order they joined.
    QList<Snowflake> usersIn(Snowflake channel) const { return m_channelUsers.value(channel); }
    // A server voice channel with people in it, or a DM / group call.
    bool hasCall(Snowflake channel) const;
    bool guildHasCall(Snowflake guild) const;
    // When a DM / group call started (its call message), in ms since the
    // epoch; 0 when not known (server voice channels).
    qint64 callStartedMs(Snowflake channel) const;

signals:
    // Something changed in this channel (guild 0: a DM or group), or, for
    // a 0 channel, everywhere (READY).
    void changed(Snowflake guild, Snowflake channel);

private:
    void loadGuildStates(Snowflake guild, const nlohmann::json& states);
    // A user is now in `channel` (0: none) in `guild` (0: DMs and groups).
    void setUserChannel(Snowflake guild, Snowflake user, Snowflake channel, bool notify = true);
    // Names of people seen only by id: ask Discord for them.
    void requestUnknownMembers(Snowflake guild, const QList<Snowflake>& users);

    QHash<QPair<Snowflake, Snowflake>, Snowflake> m_userChannel; // (guild, user) -> channel
    QHash<Snowflake, QList<Snowflake>> m_channelUsers;            // channel -> users
    QHash<Snowflake, Snowflake> m_channelGuild;                   // channel -> guild
    QHash<Snowflake, Snowflake> m_dmCalls;                        // DM / group -> call message
};
