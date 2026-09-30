#pragma once

#include <QString>

#include "discord/models/Snowflake.hpp"

class Message;

// Renders Discord message content as Qt rich text (the subset QML's Text
// supports): markdown emphasis, code, spoilers, links, mentions, custom
// emoji and timestamps.
namespace MessageFormatter {

// emojiSize: pixel size of custom emoji images.
// How a person is called in a server: their nickname there, else their
// display name, else their username; `fallback` when they are not known.
QString displayName(Snowflake user, Snowflake guild, const QString& fallback = QString());

QString richText(const QString& content, Snowflake guild, int emojiSize = 20);

// The number of emoji when the content is nothing but emoji (Unicode or
// custom, separated by spaces at most), else 0. Messages of up to three
// show them large.
int emojiOnlyCount(const QString& content);

// Plain, single-line text for reply previews and notifications.
QString plainText(const QString& content, Snowflake guild);

// A system message (joins, pins, calls, boosts, ...) as one line of rich
// text that names the author itself, with a Suru icon. Empty for messages
// that show their content.
struct SystemMessage {
    QString icon;
    QString text;
};
SystemMessage systemMessage(const Message& message, Snowflake guild);

// The same as plain text, or an empty string for regular messages.
QString systemText(const Message& message, Snowflake guild);

}
