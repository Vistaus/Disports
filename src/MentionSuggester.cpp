#include "MentionSuggester.h"

#include <QColor>

#include <algorithm>
#include <initializer_list>
#include <vector>

#include "discord/DiscordInstance.hpp"
#include "discord/models/Permissions.hpp"
#include "discord/state/ProfileCache.hpp"

#include "ChannelPermissions.h"
#include "DiscordUrls.h"
#include "Session.h"

namespace {

constexpr size_t MaxSuggestions = 8;

struct Suggestion {
    int rank; // 0: a name starts with the query, 1: contains it
    QVariantMap item;
};

// -1 when no name matches.
int rank(const QString& query, std::initializer_list<QString> names)
{
    int best = -1;
    for (const QString& name : names) {
        const QString lower = name.toLower();
        if (lower.isEmpty())
            continue;
        if (lower.startsWith(query))
            return 0;
        if (lower.contains(query))
            best = 1;
    }
    return best;
}

QString qstr(const std::string& s)
{
    return QString::fromStdString(s);
}

QVariantMap item(const QString& kind, const QString& label, const QString& detail, const QString& insert)
{
    return {
        {QStringLiteral("kind"), kind},
        {QStringLiteral("label"), label},
        {QStringLiteral("detail"), detail},
        {QStringLiteral("insert"), insert},
    };
}

void addPeople(std::vector<Suggestion>& out, const QString& query, const Channel& channel, const Guild* guild)
{
    std::vector<Snowflake> people;
    if (guild)
        people.assign(guild->m_knownMembers.begin(), guild->m_knownMembers.end());
    else
        people = channel.m_recipients;

    for (Snowflake id : people) {
        Profile* profile = GetProfileCache()->LookupProfile(id, "", "", "", false);
        if (!profile || profile->GetUsername().empty())
            continue;
        QString nick;
        if (guild) {
            auto member = profile->m_guildMembers.find(guild->m_snowflake);
            if (member != profile->m_guildMembers.end())
                nick = qstr(member->second.m_nick);
        }
        const QString username = qstr(profile->GetUsername());
        const QString global = qstr(profile->m_globalName);
        const int r = rank(query, {nick, global, username});
        if (r < 0)
            continue;
        const QString label = !nick.isEmpty() ? nick : !global.isEmpty() ? global : username;
        QVariantMap person = item(QStringLiteral("user"), label, QLatin1Char('@') + username, QLatin1Char('@') + username);
        person.insert(QStringLiteral("avatarUrl"), DiscordUrls::userAvatar(id, profile->m_avatarlnk));
        out.push_back({r, person});
    }
}

void addEveryone(std::vector<Suggestion>& out, const QString& query)
{
    const std::pair<QString, QString> everyone[] = {
        {QStringLiteral("everyone"), MentionSuggester::tr("Notify everyone who can see this channel")},
        {QStringLiteral("here"), MentionSuggester::tr("Notify everyone online who can see this channel")},
    };
    for (const auto& [name, detail] : everyone) {
        if (name.startsWith(query))
            out.push_back({0, item(QStringLiteral("everyone"), QLatin1Char('@') + name, detail, QLatin1Char('@') + name)});
    }
}

void addRoles(std::vector<Suggestion>& out, const QString& query, const Guild& guild, bool anyRole)
{
    for (const auto& [id, role] : guild.m_roles) {
        // The @everyone role has the server's id.
        if (id == guild.m_snowflake || (!role.m_bMentionable && !anyRole))
            continue;
        const QString name = qstr(role.m_name);
        const int r = rank(query, {name});
        if (r < 0)
            continue;
        QVariantMap entry = item(QStringLiteral("role"), QLatin1Char('@') + name, MentionSuggester::tr("Role"),
                                 QLatin1Char('@') + name);
        entry.insert(QStringLiteral("color"), role.m_colorOriginal ? QColor(QRgb(role.m_colorOriginal)).name() : QString());
        out.push_back({r, entry});
    }
}

void addChannels(std::vector<Suggestion>& out, const QString& query, Guild& guild)
{
    for (Channel& channel : guild.m_channels) {
        if (channel.IsCategory() || !channel.HasPermission(PERM_VIEW_CHANNEL))
            continue;
        const QString name = qstr(channel.m_name);
        const int r = rank(query, {name});
        if (r < 0)
            continue;
        Channel* category = channel.m_parentCateg ? guild.GetChannel(channel.m_parentCateg) : nullptr;
        const QString detail = category && category != &channel ? qstr(category->m_name) : QString();
        out.push_back({r, item(QStringLiteral("channel"), QLatin1Char('#') + name, detail, QLatin1Char('#') + name)});
    }
}

}

MentionSuggester::MentionSuggester(Session* session)
    : QObject(session)
    , m_session(session)
{
}

QVariantList MentionSuggester::suggestions(const QString& word) const
{
    DiscordInstance* instance = m_session->instance();
    Channel* channel = instance ? instance->GetCurrentChannel() : nullptr;
    if (!channel || word.isEmpty())
        return {};
    const QChar trigger = word.at(0);
    const QString query = word.mid(1).toLower();
    Guild* guild = channel->IsDM() ? nullptr : instance->GetGuild(channel->m_parentGuild);
    const bool everyone = m_session->permissions()->canMentionEveryone();

    std::vector<Suggestion> found;
    if (trigger == QLatin1Char('@')) {
        addPeople(found, query, *channel, guild);
        if (guild && everyone)
            addEveryone(found, query);
        if (guild)
            addRoles(found, query, *guild, everyone);
    } else if (trigger == QLatin1Char('#') && guild) {
        addChannels(found, query, *guild);
    }

    std::stable_sort(found.begin(), found.end(), [](const Suggestion& a, const Suggestion& b) {
        if (a.rank != b.rank)
            return a.rank < b.rank;
        return a.item.value(QStringLiteral("label")).toString().compare(
                   b.item.value(QStringLiteral("label")).toString(), Qt::CaseInsensitive) < 0;
    });
    QVariantList result;
    for (size_t i = 0; i < found.size() && i < MaxSuggestions; ++i)
        result.append(found[i].item);
    return result;
}

void MentionSuggester::searchMembers(const QString& query)
{
    DiscordInstance* instance = m_session->instance();
    Channel* channel = instance ? instance->GetCurrentChannel() : nullptr;
    const QString name = query.trimmed();
    if (!channel || channel->IsDM() || !m_session->connected() || name.isEmpty())
        return;
    instance->RequestGuildMembers(channel->m_parentGuild, name.toStdString(), false, 10);
}
