#include "MessageContent.h"

#include <QColor>
#include <QDateTime>
#include <QLocale>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

#include "discord/models/Message.hpp"

#include "DiscordUrls.h"
#include "MessageFormatter.h"

namespace {

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
// others (link previews, bot embeds, YouTube) as a card, see MessageContent::embeds().
bool isMediaEmbed(const RichEmbed& e)
{
    if (e.m_type == RichEmbed::GIFV)
        return e.m_bHasVideo || e.m_bHasThumbnail;
    if (e.m_type == RichEmbed::IMAGE)
        return e.m_bHasThumbnail || e.m_bHasImage;
    return e.m_bHasImage && e.m_title.empty() && e.m_description.empty() && e.m_authorName.empty()
           && e.m_providerName.empty() && e.m_fields.empty() && e.m_footerText.empty();
}

// A picture inside an embed card, in the format of MessageContent::media().
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

}

QVariantList MessageContent::media(const Message& m)
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

bool MessageContent::bodyIsEmbedLink(const Message& m)
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

QVariantList MessageContent::embeds(const Message& m, Snowflake guild, int emojiSize)
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
                {QStringLiteral("name"), MessageFormatter::richText(str(f.m_title), guild, emojiSize)},
                {QStringLiteral("value"), MessageFormatter::richText(str(f.m_value), guild, emojiSize)},
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
            {QStringLiteral("description"), MessageFormatter::richText(str(e.m_description), guild, emojiSize)},
            {QStringLiteral("fields"), fields},
            {QStringLiteral("thumbnailUrl"), thumbnail},
            {QStringLiteral("image"), image},
            {QStringLiteral("footer"), footer},
            {QStringLiteral("footerIcon"), str(e.m_footerIconProxiedUrl.empty() ? e.m_footerIconUrl : e.m_footerIconProxiedUrl)},
        });
    }
    return cards;
}

QVariantList MessageContent::stickers(const Message& m)
{
    QVariantList stickers;
    for (const StickerItem& s : m.m_stickers) {
        const bool gif = s.m_format == StickerItem::GIF;
        const bool lottie = s.m_format == StickerItem::LOTTIE;
        stickers.append(QVariantMap{
            {QStringLiteral("name"), str(s.m_name)},
            // Lottie stickers are JSON animations (qml/StickerView.qml).
            {QStringLiteral("url"), lottie ? DiscordUrls::lottieSticker(s.m_id) : DiscordUrls::sticker(s.m_id, gif)},
            {QStringLiteral("animated"), gif},
            {QStringLiteral("lottie"), lottie},
        });
    }
    return stickers;
}

QVariant MessageContent::poll(const Message& m)
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

QVariantList MessageContent::reactions(const Message& m)
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
