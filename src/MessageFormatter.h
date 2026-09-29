#pragma once

#include <QString>

#include "discord/models/Snowflake.hpp"

class Message;

// Renders Discord message content as Qt rich text (the subset QML's Text
// supports): markdown emphasis, code, spoilers, links, mentions, custom
// emoji and timestamps.
namespace MessageFormatter {

QString richText(const QString& content, Snowflake guild);

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
