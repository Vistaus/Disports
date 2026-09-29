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

// Text shown for system messages (joins, pins, calls, ...), or an empty
// string for regular messages.
QString systemText(const Message& message);

}
