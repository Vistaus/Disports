#include "GuildListModel.h"

#include <QColor>
#include <QVariantMap>

#include "discord/DiscordInstance.hpp"

#include "DiscordUrls.h"

namespace {

int guildMentions(Guild* guild)
{
    int mentions = 0;
    for (const Channel& channel : guild->m_channels)
        mentions += channel.m_mentionCount;
    return mentions;
}

QString colorName(int color)
{
    return color < 0 ? QString() : QColor::fromRgb(QRgb(color)).name();
}

}

int GuildListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant GuildListModel::data(const QModelIndex& index, int role) const
{
    DiscordInstance* instance = GetDiscordInstance();
    if (!instance || !index.isValid() || index.row() >= int(m_rows.size()))
        return QVariant();

    const Row& row = m_rows[size_t(index.row())];

    if (row.folder) {
        switch (role) {
        case KindRole:        return QStringLiteral("folder");
        case ItemIdRole:      return DiscordUrls::id(row.id);
        case NameRole:        return row.name;
        case FolderIdRole:    return DiscordUrls::id(row.id);
        case FolderColorRole: return colorName(row.color);
        case ExpandedRole:    return m_expanded.contains(row.id);
        case UnreadRole:
        case MentionsRole: {
            bool unread = false;
            int mentions = 0;
            for (Snowflake id : row.guilds) {
                if (Guild* guild = instance->GetGuild(id)) {
                    unread = unread || guild->IsUnread();
                    mentions += guildMentions(guild);
                }
            }
            return role == UnreadRole ? QVariant(unread) : QVariant(mentions);
        }
        case PreviewsRole: {
            QVariantList previews;
            for (Snowflake id : row.guilds) {
                Guild* guild = instance->GetGuild(id);
                if (!guild)
                    continue;
                const QString name = QString::fromStdString(guild->m_name);
                previews.append(QVariantMap{
                    {QStringLiteral("iconUrl"), DiscordUrls::guildIcon(id, guild->m_avatarlnk, 48)},
                    {QStringLiteral("initials"), DiscordUrls::initials(name)},
                });
                if (previews.size() == 4)
                    break;
            }
            return previews;
        }
        case GuildIdsRole: {
            QStringList ids;
            for (Snowflake id : row.guilds)
                ids.append(DiscordUrls::id(id));
            return ids;
        }
        default:
            return QVariant();
        }
    }

    Guild* guild = instance->GetGuild(row.id);
    if (!guild)
        return QVariant();
    const QString name = QString::fromStdString(guild->m_name);
    switch (role) {
    case KindRole:        return QStringLiteral("guild");
    case ItemIdRole:      return DiscordUrls::id(row.id);
    case NameRole:        return name;
    case IconUrlRole:     return DiscordUrls::guildIcon(row.id, guild->m_avatarlnk);
    case InitialsRole:    return DiscordUrls::initials(name);
    case UnreadRole:      return guild->IsUnread();
    case MentionsRole:    return guildMentions(guild);
    case FolderIdRole:    return row.folderId ? DiscordUrls::id(row.folderId) : QString();
    case FolderColorRole: return colorName(row.color);
    case ExpandedRole:    return false;
    case PreviewsRole:    return QVariantList();
    case GuildIdsRole:    return QStringList();
    }
    return QVariant();
}

QHash<int, QByteArray> GuildListModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {ItemIdRole, "itemId"},
        {NameRole, "name"},
        {IconUrlRole, "iconUrl"},
        {InitialsRole, "initials"},
        {UnreadRole, "unread"},
        {MentionsRole, "mentions"},
        {FolderIdRole, "folderId"},
        {FolderColorRole, "folderColor"},
        {ExpandedRole, "expanded"},
        {PreviewsRole, "previews"},
        {GuildIdsRole, "guildIds"},
    };
}

std::vector<GuildListModel::Row> GuildListModel::buildRows() const
{
    std::vector<Row> rows;
    DiscordInstance* instance = GetDiscordInstance();
    if (!instance)
        return rows;

    std::vector<Snowflake> listed;
    auto addGuild = [&](Snowflake id, Snowflake folderId, int color) {
        if (!instance->GetGuild(id))
            return;
        rows.push_back(Row{false, id, folderId, color, QString(), {}});
        listed.push_back(id);
    };

    for (AbstractGuildItem* item : *instance->m_guildItemList.GetItems()) {
        if (!item->IsFolder()) {
            addGuild(item->GetID(), 0, -1);
            continue;
        }

        Row folder;
        folder.folder = true;
        folder.id = item->GetID();
        folder.color = item->GetColor();
        folder.name = QString::fromStdString(instance->GetGuildFolderName(item->GetID()));
        for (AbstractGuildItem* child : *item->GetItems()) {
            if (instance->GetGuild(child->GetID()))
                folder.guilds.push_back(child->GetID());
        }
        if (folder.guilds.empty())
            continue;

        rows.push_back(folder);
        const bool expanded = m_expanded.contains(folder.id);
        for (Snowflake id : folder.guilds) {
            if (expanded)
                addGuild(id, folder.id, folder.color);
            else
                listed.push_back(id);
        }
    }

    // Guilds missing from the folder list (e.g. just joined) go last.
    for (const Guild& guild : instance->m_guilds) {
        if (std::find(listed.begin(), listed.end(), guild.m_snowflake) == listed.end())
            addGuild(guild.m_snowflake, 0, -1);
    }
    return rows;
}

void GuildListModel::reload()
{
    beginResetModel();
    m_rows = buildRows();
    endResetModel();
    emit countChanged();
    emit unreadChanged();
}

void GuildListModel::toggleFolder(const QString& folderId)
{
    const Snowflake id = DiscordUrls::fromId(folderId);
    if (m_expanded.contains(id))
        m_expanded.remove(id);
    else
        m_expanded.insert(id);

    // Insert or remove the folder's servers below it instead of resetting,
    // so the rail keeps its scroll position.
    std::vector<Row> fresh = buildRows();
    size_t i = 0;
    while (i < m_rows.size() && i < fresh.size() && m_rows[i].id == fresh[i].id
           && m_rows[i].folder == fresh[i].folder)
        ++i;
    const size_t oldCount = m_rows.size();
    const size_t newCount = fresh.size();
    if (newCount > oldCount) {
        beginInsertRows(QModelIndex(), int(i), int(i + newCount - oldCount) - 1);
        m_rows = std::move(fresh);
        endInsertRows();
    } else if (newCount < oldCount) {
        beginRemoveRows(QModelIndex(), int(i), int(i + oldCount - newCount) - 1);
        m_rows = std::move(fresh);
        endRemoveRows();
    } else {
        m_rows = std::move(fresh);
    }
    if (!m_rows.empty())
        emit dataChanged(index(0), index(int(m_rows.size()) - 1), {ExpandedRole});
    emit countChanged();
}

void GuildListModel::refreshUnread()
{
    if (!m_rows.empty())
        emit dataChanged(index(0), index(int(m_rows.size()) - 1), {UnreadRole, MentionsRole});
    emit unreadChanged();
}

void GuildListModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
    emit unreadChanged();
}

int GuildListModel::directMessageMentions() const
{
    DiscordInstance* instance = GetDiscordInstance();
    if (!instance)
        return 0;
    return guildMentions(&instance->m_dmGuild);
}
