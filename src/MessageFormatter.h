#pragma once

#include <QString>

#include "discord/models/Snowflake.hpp"

class Message;

// Renders Discord message content as Qt rich text (the subset QML's Text
// supports): markdown emphasis, code, spoilers, links, mentions, custom
// emoji and timestamps.
namespace MessageFormatter {

// Server nickname, else display name, else username; `fallback` when the
// user isn't known.
QString displayName(Snowflake user, Snowflake guild, const QString& fallback = QString());

// emojiSize: pixel size of custom emoji images.
QString richText(const QString& content, Snowflake guild, int emojiSize = 20);

// How many emoji the content is, when it is nothing but emoji (and spaces);
// else 0.
int emojiOnlyCount(const QString& content);

// Plain, single-line text for reply previews and notifications.
QString plainText(const QString& content, Snowflake guild);

// Joins, pins, calls, boosts and the like: one line of rich text naming
// the author, with a Suru icon. Empty text for regular messages.
struct SystemMessage {
    QString icon;
    QString text;
};
SystemMessage systemMessage(const Message& message, Snowflake guild);

// The same as plain text.
QString systemText(const Message& message, Snowflake guild);

}
