#pragma once

#include <QVariantMap>

#include "discord/models/Snowflake.hpp"

class Channel;
class DiscordInstance;

// For the channel info page: name, topic, kind ("dm", "group", "channel"),
// typeName, category, server, id, nsfw, iconUrl; for a 1:1 DM the other
// person: user (see describeUser()); for groups, members [{id, name,
// username, avatarUrl, blocked, self}], us first.
QVariantMap describeChannel(DiscordInstance& instance, const Channel& channel);

// For profiles (qml/ProfileView.qml): {id, name, username, avatarUrl,
// status ("online", "idle", "dnd", "offline"), customStatus, bio, pronouns,
// bot, blocked, self, profileLoaded}. Bio and pronouns come with the full
// profile (Session::userInfo() fetches it).
QVariantMap describeUser(DiscordInstance& instance, Snowflake user);
