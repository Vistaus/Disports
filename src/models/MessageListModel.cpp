#include "MessageListModel.h"

#include <QColor>
#include <QDateTime>
#include <QLocale>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

#include <list>

#include "discord/state/MessageCache.hpp"

#include "DiscordUrls.h"
#include "MessageFormatter.h"

namespace {

// Messages from the same author within this window share one header.
constexpr time_t GroupWindowSeconds = 7 * 60;

bool isSystem(const Message& message)
{
    return !MessageFormatter::systemMessage(message, 0).text.isEmpty();
}

// Discord's media proxy resizes images; ask for a preview no larger than
// needed instead of downloading the full picture into the chat.
QString previewUrl(const std::string& proxyUrl, int width, int height, int maxSize = 640)
{
    QUrl url(QString::fromStdString(proxyUrl));
    if (url.host() != QLatin1String("media.discordapp.net") || width <= 0 || height <= 0)
        return url.toString();
    if (width > maxSize || height > maxSize) {
        const double scale = double(maxSize) / double(qMax(width, height));
        width = qMax(1, int(width * scale));
        height = qMax(1, int(height * scale));
    }
    QUrlQuery query(url);
    query.removeQueryItem(QStringLiteral("width"));
    query.removeQueryItem(QStringLiteral("height"));
    query.addQueryItem(QStringLiteral("width"), QString::number(width));
    query.addQueryItem(QStringLiteral("height"), QString::number(height));
    url.setQuery(query);
    return url.toString();
}

// The media proxy serves GIFs as a still WebP unless asked for the
// animation (as an animated WebP).
QString animatedUrl(const QString& url)
{
    QUrl result(url);
    if (result.host() != QLatin1String("media.discordapp.net"))
        return url;
    QUrlQuery query(result);
    query.removeQueryItem(QStringLiteral("animated"));
    query.addQueryItem(QStringLiteral("animated"), QStringLiteral("true"));
    result.setQuery(query);
    return result.toString();
}

// A still frame of a video, served by Discord's media proxy.
QString videoThumbnailUrl(const std::string& proxyUrl, int width, int height)
{
    QUrl url(previewUrl(proxyUrl, width, height));
    if (url.host() != QLatin1String("media.discordapp.net"))
        return QString();
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("jpeg"));
    url.setQuery(query);
    return url.toString();
}

QString str(const std::string& s)
{
    return QString::fromStdString(s);
}

// Whether a URL points at a .gif file (query and fragment ignored).
bool isGifUrl(const std::string& url)
{
    return QUrl(QString::fromStdString(url)).path().endsWith(QLatin1String(".gif"), Qt::CaseInsensitive);
}

// An embed that is nothing but a picture or a GIF shows as plain media; the
// others (link previews, bot embeds, YouTube) as a card, see embedsOf().
bool isMediaEmbed(const RichEmbed& e)
{
    if (e.m_type == RichEmbed::GIFV)
        return e.m_bHasVideo || e.m_bHasThumbnail;
    if (e.m_type == RichEmbed::IMAGE)
        return e.m_bHasThumbnail || e.m_bHasImage;
    return e.m_bHasImage && e.m_title.empty() && e.m_description.empty() && e.m_authorName.empty()
           && e.m_providerName.empty() && e.m_fields.empty() && e.m_footerText.empty();
}

// A picture inside an embed card, in the format of mediaOf().
QVariantMap embedImage(const std::string& proxy, const std::string& original, int width, int height,
                       const std::string& openUrl)
{
    const std::string& source = proxy.empty() ? original : proxy;
    return {
        {QStringLiteral("kind"), QStringLiteral("image")},
        {QStringLiteral("previewUrl"), previewUrl(source, width, height)},
        {QStringLiteral("viewUrl"), str(source)},
        {QStringLiteral("viewType"), QStringLiteral("image")},
        {QStringLiteral("openUrl"), str(openUrl.empty() ? original : openUrl)},
        {QStringLiteral("fileName"), QString()},
        {QStringLiteral("width"), width},
        {QStringLiteral("height"), height},
    };
}

// What the chat shows for a message's attachments and media embeds. Nothing
// here plays by itself: the chat shows still previews (GIFs get a badge,
// videos a play button) and the viewer plays them.
//   kind:       "image" | "gif" | "video" | "file"
//   previewUrl: still image for the chat ("" for files)
//   animatedUrl: the animated version of a GIF file's preview
//   viewUrl:    what the viewer shows
//   viewType:   "image" | "animated" | "video" | "none"
//   openUrl:    for "open in browser"
QVariantList mediaOf(const Message& m)
{
    QVariantList media;
    for (const Attachment& a : m.m_attachments) {
        const std::string& proxy = a.m_proxyUrl.empty() ? a.m_actualUrl : a.m_proxyUrl;
        QString kind = QStringLiteral("file");
        QString preview, animated, viewType = QStringLiteral("none");
        QString viewUrl = str(a.m_actualUrl);
        if (a.IsVideo()) {
            kind = QStringLiteral("video");
            preview = videoThumbnailUrl(proxy, a.m_width, a.m_height);
            viewType = QStringLiteral("video");
        } else if (a.m_contentType == ContentType::GIF) {
            kind = QStringLiteral("gif");
            preview = previewUrl(proxy, a.m_width, a.m_height);
            animated = animatedUrl(preview);
            viewUrl = animatedUrl(str(proxy));
            viewType = QStringLiteral("animated");
        } else if (a.IsImage()) {
            kind = QStringLiteral("image");
            preview = previewUrl(proxy, a.m_width, a.m_height);
            viewUrl = str(proxy);
            viewType = QStringLiteral("image");
        }
        media.append(QVariantMap{
            {QStringLiteral("kind"), kind},
            {QStringLiteral("previewUrl"), preview},
            {QStringLiteral("animatedUrl"), animated},
            {QStringLiteral("viewUrl"), viewUrl},
            {QStringLiteral("viewType"), viewType},
            {QStringLiteral("openUrl"), str(a.m_actualUrl)},
            {QStringLiteral("fileName"), str(a.m_fileName)},
            {QStringLiteral("width"), a.m_width},
            {QStringLiteral("height"), a.m_height},
        });
    }

    for (const RichEmbed& e : m.m_embeds) {
        if (!isMediaEmbed(e))
            continue;
        QVariantMap item;
        const std::string videoUrl = e.m_videoProxiedUrl.empty() ? e.m_videoUrl : e.m_videoProxiedUrl;
        if (e.m_type == RichEmbed::GIFV && !videoUrl.empty()) {
            // "GIFs" from Tenor, Giphy, Imgur and co. are really short
            // looping videos: play them as muted video, never as images.
            item = {
                {QStringLiteral("kind"), QStringLiteral("gif")},
                {QStringLiteral("previewUrl"), e.m_bHasThumbnail
                     ? previewUrl(e.m_thumbnailProxiedUrl, e.m_thumbnailWidth, e.m_thumbnailHeight)
                     : QString()},
                {QStringLiteral("viewUrl"), str(videoUrl)},
                {QStringLiteral("viewType"), QStringLiteral("video")},
                {QStringLiteral("width"), e.m_videoWidth ? e.m_videoWidth : e.m_thumbnailWidth},
                {QStringLiteral("height"), e.m_videoHeight ? e.m_videoHeight : e.m_thumbnailHeight},
            };
        } else if ((e.m_type == RichEmbed::IMAGE || e.m_type == RichEmbed::GIFV)
                   && (e.m_bHasThumbnail || e.m_bHasImage)) {
            // A picture link, or a GIF embed without a video: a direct link
            // to a .gif file animates, anything else is a still picture.
            const bool thumb = e.m_bHasThumbnail;
            const std::string& proxy = thumb ? e.m_thumbnailProxiedUrl : e.m_imageProxiedUrl;
            const std::string& original = thumb ? e.m_thumbnailUrl : e.m_imageUrl;
            const std::string& source = proxy.empty() ? original : proxy;
            const int w = thumb ? e.m_thumbnailWidth : e.m_imageWidth;
            const int h = thumb ? e.m_thumbnailHeight : e.m_imageHeight;
            const bool gif = isGifUrl(original) || isGifUrl(e.m_url);
            item = {
                {QStringLiteral("kind"), gif ? QStringLiteral("gif") : QStringLiteral("image")},
                {QStringLiteral("previewUrl"), previewUrl(source, w, h)},
                {QStringLiteral("animatedUrl"), gif ? animatedUrl(previewUrl(source, w, h)) : QString()},
                {QStringLiteral("viewUrl"), gif ? animatedUrl(str(source)) : str(source)},
                {QStringLiteral("viewType"), gif ? QStringLiteral("animated") : QStringLiteral("image")},
                {QStringLiteral("width"), w},
                {QStringLiteral("height"), h},
            };
        } else if (e.m_bHasImage) {
            item = {
                {QStringLiteral("kind"), QStringLiteral("image")},
                {QStringLiteral("previewUrl"), previewUrl(e.m_imageProxiedUrl, e.m_imageWidth, e.m_imageHeight)},
                {QStringLiteral("viewUrl"), str(e.m_imageProxiedUrl)},
                {QStringLiteral("viewType"), QStringLiteral("image")},
                {QStringLiteral("width"), e.m_imageWidth},
                {QStringLiteral("height"), e.m_imageHeight},
            };
        } else {
            continue;
        }
        item.insert(QStringLiteral("openUrl"), str(e.m_url));
        item.insert(QStringLiteral("fileName"), str(e.m_title));
        media.append(item);
    }
    return media;
}

// A message that is only the link of its own GIF or image embed shows just
// the media, like Discord.
bool bodyIsEmbedLink(const Message& m)
{
    const QString content = str(m.m_message).trimmed();
    if (content.isEmpty() || content.contains(QLatin1Char(' ')) || !content.startsWith(QLatin1String("http")))
        return false;
    for (const RichEmbed& e : m.m_embeds) {
        if ((e.m_type == RichEmbed::GIFV || e.m_type == RichEmbed::IMAGE) && str(e.m_url) == content)
            return true;
    }
    return false;
}

// Link previews, bot embeds and videos from other sites (YouTube), shown as
// cards. Media-only embeds are in mediaOf() instead.
//   color, provider, author, authorUrl, authorIcon, title, url,
//   description (rich text), fields [{name, value, inline}],
//   thumbnailUrl (small picture beside the text), image (mediaOf() entry
//   under the text, or null), footer, footerIcon
QVariantList embedsOf(const Message& m, Snowflake guild)
{
    QVariantList cards;
    for (const RichEmbed& e : m.m_embeds) {
        if (isMediaEmbed(e))
            continue;
        if (e.m_title.empty() && e.m_description.empty() && e.m_authorName.empty() && e.m_fields.empty()
                && !e.m_bHasImage && !e.m_bHasThumbnail)
            continue;

        QVariantList fields;
        for (const RichEmbedField& f : e.m_fields) {
            fields.append(QVariantMap{
                {QStringLiteral("name"), MessageFormatter::richText(str(f.m_title), guild)},
                {QStringLiteral("value"), MessageFormatter::richText(str(f.m_value), guild)},
                {QStringLiteral("inline"), f.m_bInline},
            });
        }

        QVariant image;
        QString thumbnail;
        if (e.m_type == RichEmbed::VIDEO && e.m_bHasThumbnail) {
            // YouTube and co.: the viewer cannot play these, so it opens the page.
            QVariantMap video = embedImage(e.m_thumbnailProxiedUrl, e.m_thumbnailUrl,
                                           e.m_thumbnailWidth, e.m_thumbnailHeight, e.m_url);
            video[QStringLiteral("kind")] = QStringLiteral("video");
            video[QStringLiteral("viewType")] = QStringLiteral("none");
            image = video;
        } else if (e.m_bHasImage) {
            image = embedImage(e.m_imageProxiedUrl, e.m_imageUrl, e.m_imageWidth, e.m_imageHeight, e.m_imageUrl);
        } else if (e.m_bHasThumbnail && e.m_type == RichEmbed::ARTICLE) {
            // Articles show their picture large, like Discord.
            image = embedImage(e.m_thumbnailProxiedUrl, e.m_thumbnailUrl,
                               e.m_thumbnailWidth, e.m_thumbnailHeight, e.m_thumbnailUrl);
        } else if (e.m_bHasThumbnail) {
            thumbnail = previewUrl(e.m_thumbnailProxiedUrl.empty() ? e.m_thumbnailUrl : e.m_thumbnailProxiedUrl,
                                   e.m_thumbnailWidth, e.m_thumbnailHeight, 160);
        }

        QString footer = str(e.m_footerText);
        if (e.m_timestamp > 0) {
            const QString time = QLocale().toString(QDateTime::fromSecsSinceEpoch(e.m_timestamp), QLocale::ShortFormat);
            footer = footer.isEmpty() ? time : footer + QStringLiteral(" • ") + time;
        }

        cards.append(QVariantMap{
            {QStringLiteral("color"), e.m_color ? QColor(QRgb(e.m_color)).name() : QString()},
            {QStringLiteral("provider"), str(e.m_providerName)},
            {QStringLiteral("author"), str(e.m_authorName)},
            {QStringLiteral("authorUrl"), str(e.m_authorUrl)},
            {QStringLiteral("authorIcon"), str(e.m_authorIconProxiedUrl.empty() ? e.m_authorIconUrl : e.m_authorIconProxiedUrl)},
            {QStringLiteral("title"), str(e.m_title).toHtmlEscaped()},
            {QStringLiteral("url"), str(e.m_url)},
            {QStringLiteral("description"), MessageFormatter::richText(str(e.m_description), guild)},
            {QStringLiteral("fields"), fields},
            {QStringLiteral("thumbnailUrl"), thumbnail},
            {QStringLiteral("image"), image},
            {QStringLiteral("footer"), footer},
            {QStringLiteral("footerIcon"), str(e.m_footerIconProxiedUrl.empty() ? e.m_footerIconUrl : e.m_footerIconProxiedUrl)},
        });
    }
    return cards;
}

QVariantList stickersOf(const Message& m)
{
    QVariantList stickers;
    for (const StickerItem& s : m.m_stickers) {
        const bool gif = s.m_format == StickerItem::GIF;
        const bool lottie = s.m_format == StickerItem::LOTTIE;
        stickers.append(QVariantMap{
            {QStringLiteral("name"), str(s.m_name)},
            // Lottie stickers are animations Qt cannot draw; they show
            // their name instead.
            {QStringLiteral("url"), lottie ? QString() : DiscordUrls::sticker(s.m_id, gif)},
            {QStringLiteral("animated"), gif},
            {QStringLiteral("lottie"), lottie},
        });
    }
    return stickers;
}

// A poll: {question, multiselect, closed, expires (text), totalVotes,
// answers: [{id, text, emoji, emojiUrl, votes, percent, me}]}
QVariant pollOf(const Message& m)
{
    if (!m.m_pMessagePoll)
        return QVariant();
    const MessagePoll& poll = *m.m_pMessagePoll;

    int total = 0;
    for (const auto& [id, option] : poll.m_options)
        total += option.m_voteCount;

    QVariantList answers;
    for (const auto& [id, option] : poll.m_options) {
        answers.append(QVariantMap{
            {QStringLiteral("id"), option.m_answerId},
            {QStringLiteral("text"), str(option.m_text)},
            {QStringLiteral("emoji"), option.m_emojiSF ? QString() : str(option.m_emojiUTF8)},
            {QStringLiteral("emojiUrl"), option.m_emojiSF ? DiscordUrls::emoji(option.m_emojiSF, false) : QString()},
            {QStringLiteral("votes"), option.m_voteCount},
            {QStringLiteral("percent"), total > 0 ? qRound(100.0 * option.m_voteCount / total) : 0},
            {QStringLiteral("me"), option.m_bMeVoted},
        });
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const bool closed = poll.m_bIsFinalized || (poll.m_expiry > 0 && poll.m_expiry <= now);
    QString expires;
    if (!closed && poll.m_expiry > 0) {
        const qint64 left = poll.m_expiry - now;
        // Rounded up: "3h left" until it is under 2 hours.
        if (left < 3600)
            expires = QStringLiteral("%1m left").arg(qMax<qint64>(1, (left + 59) / 60));
        else if (left < 86400)
            expires = QStringLiteral("%1h left").arg((left + 3599) / 3600);
        else
            expires = QStringLiteral("%1d left").arg((left + 86399) / 86400);
    }

    return QVariantMap{
        {QStringLiteral("question"), str(poll.m_question)},
        {QStringLiteral("multiselect"), poll.m_bAllowMultiselect},
        {QStringLiteral("closed"), closed},
        {QStringLiteral("expires"), expires},
        {QStringLiteral("totalVotes"), total},
        {QStringLiteral("answers"), answers},
    };
}

QVariantList reactionsOf(const Message& m)
{
    QVariantList list;
    for (const Reaction& r : m.m_reactions) {
        list.append(QVariantMap{
            {QStringLiteral("emoji"), str(r.GetApiString())},
            {QStringLiteral("text"), r.m_emojiId ? QString() : str(r.m_emojiName)},
            {QStringLiteral("imageUrl"), r.m_emojiId ? DiscordUrls::emoji(r.m_emojiId, false) : QString()},
            {QStringLiteral("count"), r.m_count},
            {QStringLiteral("me"), r.m_bMe},
        });
    }
    return list;
}

bool sameRows(const std::vector<MessagePtr>& a, size_t aFrom,
              const std::vector<MessagePtr>& b, size_t bFrom, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (a[aFrom + i]->m_snowflake != b[bFrom + i]->m_snowflake)
            return false;
    }
    return true;
}

}

int MessageListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant MessageListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= int(m_rows.size()))
        return QVariant();

    const Row& row = m_rows[size_t(index.row())];
    const Message& m = *row.message;
    const MessageFormatter::SystemMessage system = MessageFormatter::systemMessage(m, m_guild);
    const bool systemRow = !system.text.isEmpty();

    switch (role) {
    case MessageIdRole: return DiscordUrls::id(m.m_snowflake);
    case AuthorIdRole:  return DiscordUrls::id(m.m_author_snowflake);
    case AuthorRole:    return QString::fromStdString(m.m_author);
    case AvatarUrlRole: return DiscordUrls::userAvatar(m.m_author_snowflake, m.m_avatar);
    case BodyRole:
        if (systemRow)
            return system.text;
        if (m.m_type == MessageType::THREAD_STARTER_MESSAGE)
            return m.m_pReferencedMessage
                       ? MessageFormatter::richText(str(m.m_pReferencedMessage->m_message), m_guild)
                       : QStringLiteral("<i>Sorry, we couldn't load the first message in this thread.</i>");
        if (m.m_bIsForward && m.m_pReferencedMessage)
            return MessageFormatter::richText(str(m.m_pReferencedMessage->m_message), m_guild);
        return bodyIsEmbedLink(m) ? QString() : richBody(m);
    case PlainBodyRole:
        if (systemRow)
            return MessageFormatter::systemText(m, m_guild);
        if (m.m_bIsForward && m.m_pReferencedMessage)
            return str(m.m_pReferencedMessage->m_message);
        return str(m.m_message);
    case TimestampRole:
        return m.m_type == MessageType::SENDING_MESSAGE ? QStringLiteral("Sending…")
                                                        : QString::fromStdString(m.m_dateCompact);
    case EditedRole:    return m.m_timeEdited != 0;
    case IsOwnRole:     return m_ownUser != 0 && m.m_author_snowflake == m_ownUser;
    case IsPendingRole: return m.m_type == MessageType::SENDING_MESSAGE;
    case IsSystemRole:  return systemRow;
    case SystemIconRole: return system.icon;
    case GroupedRole:   return row.grouped;
    case HasReplyRole:  return m.IsReply() && m.m_type != MessageType::THREAD_STARTER_MESSAGE && !systemRow;
    case ReplyAuthorRole:
        return m.m_pReferencedMessage ? QString::fromStdString(m.m_pReferencedMessage->m_author) : QString();
    case ReplyBodyRole:
        return m.m_pReferencedMessage
                   ? MessageFormatter::plainText(QString::fromStdString(m.m_pReferencedMessage->m_message), m_guild)
                   : QString();
    // System messages carry their data in embeds (AutoMod, poll results);
    // their line already says it all.
    case MediaRole:     return systemRow ? QVariantList() : mediaOf(m);
    case ReactionsRole: return reactionsOf(m);
    case EmbedsRole:    return systemRow ? QVariantList() : embedsOf(m, m_guild);
    case InteractionRole:
        if (m.m_interactionName.empty() || m.m_interactionUserName.empty())
            return QString();
        return m.m_type == MessageType::CONTEXT_MENU_COMMAND
                   ? QStringLiteral("<b>%1</b> used %2").arg(str(m.m_interactionUserName).toHtmlEscaped(),
                                                             str(m.m_interactionName).toHtmlEscaped())
                   : QStringLiteral("<b>%1</b> used <b>/%2</b>").arg(str(m.m_interactionUserName).toHtmlEscaped(),
                                                                     str(m.m_interactionName).toHtmlEscaped());
    case ForwardedRole: return m.m_bIsForward;
    case StickersRole:  return systemRow ? QVariantList() : stickersOf(m);
    case PollRole:      return pollOf(m);
    }
    return QVariant();
}

QHash<int, QByteArray> MessageListModel::roleNames() const
{
    return {
        {MessageIdRole, "messageId"},
        {AuthorIdRole, "authorId"},
        {AuthorRole, "author"},
        {AvatarUrlRole, "avatarUrl"},
        {BodyRole, "body"},
        {PlainBodyRole, "plainBody"},
        {TimestampRole, "timestamp"},
        {EditedRole, "edited"},
        {IsOwnRole, "isOwn"},
        {IsPendingRole, "isPending"},
        {IsSystemRole, "isSystem"},
        {GroupedRole, "grouped"},
        {HasReplyRole, "hasReply"},
        {ReplyAuthorRole, "replyAuthor"},
        {ReplyBodyRole, "replyBody"},
        {MediaRole, "media"},
        {ReactionsRole, "reactions"},
        {EmbedsRole, "embeds"},
        {SystemIconRole, "systemIcon"},
        {InteractionRole, "interaction"},
        {ForwardedRole, "forwarded"},
        {StickersRole, "stickers"},
        {PollRole, "poll"},
    };
}

void MessageListModel::setChannel(Snowflake guild, Snowflake channel)
{
    if (m_channel == channel && m_guild == guild)
        return;
    m_guild = guild;
    m_channel = channel;
    clear();
    sync();
}

void MessageListModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_bodyCache.clear();
    endResetModel();
    m_olderGap = 0;
    m_reachedStart = false;
    emit countChanged();
    emit hasOlderChanged();
}

std::vector<MessageListModel::Row> MessageListModel::readCache(Snowflake& olderGap, bool& reachedStart) const
{
    olderGap = 0;
    reachedStart = false;
    std::vector<Row> rows;
    if (!m_channel)
        return rows;

    std::list<MessagePtr> cached;
    GetMessageCache()->GetLoadedMessages(m_channel, m_guild, cached);

    // The cache is ordered oldest first; rows are newest first.
    for (auto it = cached.rbegin(); it != cached.rend(); ++it) {
        const MessagePtr& message = *it;
        if (message->m_type == MessageType::GAP_UP) {
            // Keep the oldest gap: that is where older history continues.
            olderGap = message->m_snowflake;
            continue;
        }
        if (message->m_type == MessageType::CHANNEL_HEADER) {
            reachedStart = true;
            continue;
        }
        if (message->IsLoadGap())
            continue;
        rows.push_back(Row{message, false});
    }
    computeGrouping(rows);
    return rows;
}

void MessageListModel::computeGrouping(std::vector<Row>& rows)
{
    for (size_t i = 0; i < rows.size(); ++i) {
        rows[i].grouped = false;
        if (i + 1 >= rows.size())
            continue;
        const Message& current = *rows[i].message;
        const Message& older = *rows[i + 1].message;
        if (current.m_author_snowflake != older.m_author_snowflake)
            continue;
        if (current.IsReply() || isSystem(current) || isSystem(older))
            continue;
        if (current.m_dateTime - older.m_dateTime > GroupWindowSeconds)
            continue;
        rows[i].grouped = true;
    }
}

void MessageListModel::sync()
{
    Snowflake olderGap = 0;
    bool reachedStart = false;
    std::vector<Row> fresh = readCache(olderGap, reachedStart);

    std::vector<MessagePtr> oldIds, newIds;
    for (const Row& row : m_rows)
        oldIds.push_back(row.message);
    for (const Row& row : fresh)
        newIds.push_back(row.message);

    const size_t oldCount = oldIds.size();
    const size_t newCount = newIds.size();

    // Work out where the old rows sit inside the new list, so that new
    // messages (front) and older history (back) are inserted instead of
    // resetting the view.
    size_t front = 0;
    bool aligned = false;
    if (oldCount > 0 && newCount >= oldCount) {
        for (size_t offset = 0; offset + oldCount <= newCount; ++offset) {
            if (newIds[offset]->m_snowflake == oldIds[0]->m_snowflake) {
                if (sameRows(newIds, offset, oldIds, 0, oldCount)) {
                    front = offset;
                    aligned = true;
                }
                break;
            }
        }
    }

    // Only deletions: the new list is the old one with some rows missing.
    bool onlyRemovals = false;
    if (!aligned && oldCount > 0 && newCount < oldCount) {
        size_t j = 0;
        for (size_t i = 0; i < oldCount && j < newCount; ++i) {
            if (oldIds[i]->m_snowflake == newIds[j]->m_snowflake)
                ++j;
        }
        onlyRemovals = j == newCount;
    }

    if (onlyRemovals) {
        size_t j = 0;
        for (size_t i = 0; i < m_rows.size();) {
            if (j < fresh.size() && m_rows[i].message->m_snowflake == fresh[j].message->m_snowflake) {
                m_rows[i] = fresh[j];
                ++i;
                ++j;
                continue;
            }
            beginRemoveRows(QModelIndex(), int(i), int(i));
            m_bodyCache.remove(m_rows[i].message->m_snowflake);
            m_rows.erase(m_rows.begin() + long(i));
            endRemoveRows();
        }
        if (!m_rows.empty())
            emit dataChanged(index(0), index(int(m_rows.size()) - 1), {GroupedRole});
        emit countChanged();
    } else if (!aligned) {
        beginResetModel();
        m_rows = std::move(fresh);
        endResetModel();
        emit countChanged();
    } else {
        const size_t back = newCount - oldCount - front;
        if (front > 0) {
            beginInsertRows(QModelIndex(), 0, int(front) - 1);
            m_rows.insert(m_rows.begin(), fresh.begin(), fresh.begin() + long(front));
            endInsertRows();
        }
        if (back > 0) {
            const int first = int(m_rows.size());
            beginInsertRows(QModelIndex(), first, first + int(back) - 1);
            m_rows.insert(m_rows.end(), fresh.end() - long(back), fresh.end());
            endInsertRows();
        }
        // Content (edits, embeds, grouping) may have changed for existing
        // rows; message objects are replaced on edit, so refresh them all.
        for (size_t i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].message != fresh[i].message)
                m_bodyCache.remove(fresh[i].message->m_snowflake);
            m_rows[i] = fresh[i];
        }
        if (!m_rows.empty())
            emit dataChanged(index(0), index(int(m_rows.size()) - 1));
        if (front > 0 || back > 0)
            emit countChanged();
    }

    if (olderGap != m_olderGap || reachedStart != m_reachedStart) {
        m_olderGap = olderGap;
        m_reachedStart = reachedStart;
        emit hasOlderChanged();
    }
}

MessagePtr MessageListModel::olderGap() const
{
    if (!m_olderGap || !m_channel)
        return nullptr;
    return GetMessageCache()->GetLoadedMessage(m_channel, m_olderGap);
}

Snowflake MessageListModel::newestMessageId() const
{
    for (const Row& row : m_rows) {
        if (row.message->m_type != MessageType::SENDING_MESSAGE)
            return row.message->m_snowflake;
    }
    return 0;
}

QString MessageListModel::richBody(const Message& message) const
{
    auto it = m_bodyCache.constFind(message.m_snowflake);
    if (it != m_bodyCache.constEnd())
        return *it;
    const QString body = MessageFormatter::richText(QString::fromStdString(message.m_message), m_guild);
    m_bodyCache.insert(message.m_snowflake, body);
    return body;
}
