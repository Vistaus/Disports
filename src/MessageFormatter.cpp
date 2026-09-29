#include "MessageFormatter.h"

#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>
#include <QStringList>

#include "discord/DiscordInstance.hpp"
#include "discord/models/Message.hpp"

#include "DiscordUrls.h"

namespace {

QString userName(Snowflake user, Snowflake guild)
{
    DiscordInstance* instance = GetDiscordInstance();
    if (!instance)
        return QStringLiteral("user");
    return QString::fromStdString(instance->LookupUserNameGlobally(user, guild));
}

QString channelName(Snowflake channel)
{
    DiscordInstance* instance = GetDiscordInstance();
    if (!instance)
        return QStringLiteral("channel");
    return QString::fromStdString(instance->LookupChannelNameGlobally(channel));
}

QString roleName(Snowflake role, Snowflake guild)
{
    DiscordInstance* instance = GetDiscordInstance();
    if (!instance)
        return QStringLiteral("role");
    return QString::fromStdString(instance->LookupRoleName(role, guild));
}

QString formatTimestamp(qint64 seconds, const QString& style)
{
    const QDateTime time = QDateTime::fromSecsSinceEpoch(seconds);
    const QLocale locale;
    if (style == QLatin1String("t"))
        return locale.toString(time.time(), QLocale::ShortFormat);
    if (style == QLatin1String("T"))
        return locale.toString(time.time(), QLocale::LongFormat);
    if (style == QLatin1String("d"))
        return locale.toString(time.date(), QLocale::ShortFormat);
    if (style == QLatin1String("D"))
        return locale.toString(time.date(), QLocale::LongFormat);
    if (style == QLatin1String("R")) {
        const qint64 diff = QDateTime::currentSecsSinceEpoch() - seconds;
        const qint64 abs = qAbs(diff);
        QString amount;
        if (abs < 60)
            amount = QStringLiteral("%1 seconds").arg(abs);
        else if (abs < 3600)
            amount = QStringLiteral("%1 minutes").arg(abs / 60);
        else if (abs < 86400)
            amount = QStringLiteral("%1 hours").arg(abs / 3600);
        else
            amount = QStringLiteral("%1 days").arg(abs / 86400);
        return diff >= 0 ? amount + QStringLiteral(" ago") : QStringLiteral("in ") + amount;
    }
    return locale.toString(time, QLocale::ShortFormat);
}

// Generated HTML is swapped for placeholders while the markdown rules run,
// so they cannot rewrite URLs or names inside it.
struct Protected {
    QStringList html;

    QString add(const QString& fragment)
    {
        html.append(fragment);
        return QChar(0xE000) + QString::number(html.size() - 1) + QChar(0xE001);
    }

    QString restore(QString text) const
    {
        static const QRegularExpression placeholder(QStringLiteral("\\x{E000}(\\d+)\\x{E001}"));
        QString out;
        qsizetype last = 0;
        auto it = placeholder.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            out += text.mid(last, m.capturedStart() - last);
            out += html.value(m.captured(1).toInt());
            last = m.capturedEnd();
        }
        out += text.mid(last);
        return out;
    }
};

// Links, mentions, custom emoji and timestamps, applied to HTML-escaped text
// (so "<" and ">" appear as entities). In rich mode the generated HTML is
// kept in `store`.
QString replaceTokens(QString text, Snowflake guild, bool rich, Protected* store = nullptr)
{
    static const QRegularExpression user(QStringLiteral("&lt;@!?(\\d+)&gt;"));
    static const QRegularExpression channel(QStringLiteral("&lt;#(\\d+)&gt;"));
    static const QRegularExpression role(QStringLiteral("&lt;@&amp;(\\d+)&gt;"));
    static const QRegularExpression emoji(QStringLiteral("&lt;(a?):(\\w+):(\\d+)&gt;"));
    static const QRegularExpression timestamp(QStringLiteral("&lt;t:(-?\\d+)(?::([tTdDfFR]))?&gt;"));

    auto replaceAll = [&text](const QRegularExpression& re, auto&& make) {
        QString out;
        qsizetype last = 0;
        auto it = re.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            out += text.mid(last, m.capturedStart() - last);
            out += make(m);
            last = m.capturedEnd();
        }
        out += text.mid(last);
        text = out;
    };

    static const QRegularExpression link(QStringLiteral("https?://[^\\s<]+[^\\s<.,:;\"')\\]]"));

    auto html = [store](const QString& fragment) {
        return store ? store->add(fragment) : fragment;
    };
    auto mention = [rich, &html](const QString& label) {
        return rich ? html(QStringLiteral("<b>%1</b>").arg(label.toHtmlEscaped())) : label;
    };

    if (rich) {
        replaceAll(link, [&](const QRegularExpressionMatch& m) {
            return html(QStringLiteral("<a href=\"%1\">%1</a>").arg(m.captured(0)));
        });
    }

    replaceAll(user, [&](const QRegularExpressionMatch& m) {
        return mention(QLatin1Char('@') + userName(DiscordUrls::fromId(m.captured(1)), guild));
    });
    replaceAll(role, [&](const QRegularExpressionMatch& m) {
        return mention(QLatin1Char('@') + roleName(DiscordUrls::fromId(m.captured(1)), guild));
    });
    replaceAll(channel, [&](const QRegularExpressionMatch& m) {
        return mention(QLatin1Char('#') + channelName(DiscordUrls::fromId(m.captured(1))));
    });
    replaceAll(emoji, [&](const QRegularExpressionMatch& m) {
        if (!rich)
            return QStringLiteral(":%1:").arg(m.captured(2));
        const bool animated = !m.captured(1).isEmpty();
        return html(QStringLiteral("<img src=\"%1\" width=\"20\" height=\"20\">")
                        .arg(DiscordUrls::emoji(DiscordUrls::fromId(m.captured(3)), animated)));
    });
    replaceAll(timestamp, [&](const QRegularExpressionMatch& m) {
        const QString formatted = formatTimestamp(m.captured(1).toLongLong(), m.captured(2));
        return rich ? html(QStringLiteral("<b>%1</b>").arg(formatted.toHtmlEscaped())) : formatted;
    });
    return text;
}

// Emphasis, strike and spoilers on escaped text without code.
QString applyInline(QString text)
{
    static const QRegularExpression bold(QStringLiteral("\\*\\*(.+?)\\*\\*"));
    static const QRegularExpression underline(QStringLiteral("__(.+?)__"));
    static const QRegularExpression italicStar(QStringLiteral("(?<![\\w*])\\*(?!\\s)(.+?)(?<!\\s)\\*(?![\\w*])"));
    static const QRegularExpression italicUnderscore(QStringLiteral("(?<![\\w_])_(?!\\s)(.+?)(?<!\\s)_(?![\\w_])"));
    static const QRegularExpression strike(QStringLiteral("~~(.+?)~~"));
    static const QRegularExpression spoiler(QStringLiteral("\\|\\|(.+?)\\|\\|"));

    text.replace(bold, QStringLiteral("<b>\\1</b>"));
    text.replace(underline, QStringLiteral("<u>\\1</u>"));
    text.replace(italicStar, QStringLiteral("<i>\\1</i>"));
    text.replace(italicUnderscore, QStringLiteral("<i>\\1</i>"));
    text.replace(strike, QStringLiteral("<s>\\1</s>"));
    // Spoilers are shown dimmed rather than hidden until tapped (for now).
    text.replace(spoiler, QStringLiteral("<span style=\"background-color:#555;color:#ddd\">\\1</span>"));
    return text;
}

// Headings, block quotes and list bullets, per line.
QString applyBlocks(const QString& text)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString& line : lines) {
        if (line.startsWith(QLatin1String("### ")))
            line = QStringLiteral("<b>%1</b>").arg(line.mid(4));
        else if (line.startsWith(QLatin1String("## ")))
            line = QStringLiteral("<b><big>%1</big></b>").arg(line.mid(3));
        else if (line.startsWith(QLatin1String("# ")))
            line = QStringLiteral("<b><big><big>%1</big></big></b>").arg(line.mid(2));
        else if (line.startsWith(QLatin1String("&gt; ")))
            line = QStringLiteral("<font color=\"#888\">▎</font> %1").arg(line.mid(5));
        else if (line.startsWith(QLatin1String("- ")) || line.startsWith(QLatin1String("* ")))
            line = QStringLiteral("• %1").arg(line.mid(2));
    }
    return lines.join(QStringLiteral("<br>"));
}

}

namespace MessageFormatter {

QString richText(const QString& content, Snowflake guild)
{
    // Split out code first so nothing inside it is formatted.
    static const QRegularExpression code(QStringLiteral("```(?:[\\w+-]*\\n)?([\\s\\S]*?)```|`([^`\\n]+)`"));

    QString result;
    qsizetype last = 0;
    auto flushText = [&](qsizetype end) {
        Protected store;
        const QString escaped = content.mid(last, end - last).toHtmlEscaped();
        result += store.restore(applyBlocks(applyInline(replaceTokens(escaped, guild, true, &store))));
    };

    auto it = code.globalMatch(content);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        flushText(m.capturedStart());
        if (m.capturedLength(1) > 0 || m.captured(0).startsWith(QLatin1String("```"))) {
            QString block = m.captured(1).toHtmlEscaped();
            if (block.endsWith(QLatin1Char('\n')))
                block.chop(1);
            block.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
            block.replace(QLatin1Char(' '), QStringLiteral("&nbsp;"));
            result += QStringLiteral("<br><font face=\"Ubuntu Mono,monospace\">%1</font><br>").arg(block);
        } else {
            result += QStringLiteral("<font face=\"Ubuntu Mono,monospace\">%1</font>")
                          .arg(m.captured(2).toHtmlEscaped());
        }
        last = m.capturedEnd();
    }
    flushText(content.size());
    return result;
}

QString plainText(const QString& content, Snowflake guild)
{
    QString text = replaceTokens(content.toHtmlEscaped(), guild, false);
    // Undo the escaping: the result is shown as plain text.
    text.replace(QLatin1String("&lt;"), QLatin1String("<"));
    text.replace(QLatin1String("&gt;"), QLatin1String(">"));
    text.replace(QLatin1String("&quot;"), QLatin1String("\""));
    text.replace(QLatin1String("&amp;"), QLatin1String("&"));
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return text;
}

QString systemText(const Message& message)
{
    using namespace MessageType;
    switch (message.m_type) {
    case RECIPIENT_ADD:          return QStringLiteral("added someone to the group.");
    case RECIPIENT_REMOVE:       return QStringLiteral("removed someone from the group.");
    case CALL:                   return QStringLiteral("started a call.");
    case CHANNEL_NAME_CHANGE:    return QStringLiteral("changed the channel name.");
    case CHANNEL_ICON_CHANGE:    return QStringLiteral("changed the channel icon.");
    case CHANNEL_PINNED_MESSAGE: return QStringLiteral("pinned a message to this channel.");
    case USER_JOIN:              return QStringLiteral("joined the server.");
    case GUILD_BOOST:
    case GUILD_BOOST_TIER_1:
    case GUILD_BOOST_TIER_2:
    case GUILD_BOOST_TIER_3:     return QStringLiteral("boosted the server!");
    case CHANNEL_FOLLOW_ADD:     return QStringLiteral("followed a channel.");
    case THREAD_CREATED:         return QStringLiteral("started a thread.");
    case STAGE_START:            return QStringLiteral("started a stage.");
    case STAGE_END:              return QStringLiteral("ended the stage.");
    default:                     return QString();
    }
}

}
