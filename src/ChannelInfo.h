#pragma once

#include <QVariantMap>

class Channel;
class DiscordInstance;

// For the channel info page: name, topic, kind ("dm", "group", "channel"),
// typeName, category, server, id, nsfw, iconUrl and, for direct messages
// and groups, members [{id, name, username, avatarUrl, blocked}].
QVariantMap describeChannel(DiscordInstance& instance, const Channel& channel);
