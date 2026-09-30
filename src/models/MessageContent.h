#pragma once

#include <QVariant>

#include "discord/models/Snowflake.hpp"

class Message;

// A message's attachments, embeds, stickers, poll and reactions, as the
// chat's QML shows them.
namespace MessageContent {

// Attachments and media embeds. The chat shows still previews (GIFs get a
// badge, videos a play button); the viewer plays them.
//   kind:        "image" | "gif" | "video" | "file"
//   previewUrl:  still image for the chat ("" for files)
//   animatedUrl: the animated version of a GIF's preview
//   viewUrl:     what the viewer shows
//   viewType:    "image" | "animated" | "video" | "none"
//   openUrl:     for "open in browser"
QVariantList media(const Message& m);

// Link previews, bot embeds and videos from other sites, shown as cards
// (media-only embeds are in media()):
//   color, provider, author, authorUrl, authorIcon, title, url,
//   description (rich text), fields [{name, value, inline}],
//   thumbnailUrl (beside the text), image (a media() entry under the
//   text, or null), footer, footerIcon
QVariantList embeds(const Message& m, Snowflake guild, int emojiSize);

// [{name, url, animated, lottie}]
QVariantList stickers(const Message& m);

// {question, multiselect, closed, expires (text), totalVotes,
//  answers: [{id, text, emoji, emojiUrl, votes, percent, me}]}, or null.
QVariant poll(const Message& m);

// [{emoji, text, imageUrl, count, me}]
QVariantList reactions(const Message& m);

// The message is just the link of its own GIF or image embed, so only the
// media shows.
bool bodyIsEmbedLink(const Message& m);

}
