#include "EmojiPickerModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

#include "discord/DiscordInstance.hpp"

#include "DiscordUrls.h"

namespace {

struct Category {
    const char* key;
    const char* label;
    const char* icon; // a representative emoji
};

const Category Categories[] = {
    {"server", "Server", ""},
    {"faces", "Smileys", "😀"},
    {"people", "People", "👋"},
    {"nature", "Nature", "🐶"},
    {"food", "Food", "🍔"},
    {"activities", "Activities", "⚽"},
    {"travel", "Travel", "🚗"},
    {"objects", "Objects", "💡"},
    {"symbols", "Symbols", "❤️"},
    {"flags", "Flags", "🏳️"},
};

}

EmojiPickerModel::EmojiPickerModel(QObject* parent)
    : QAbstractListModel(parent)
{
    loadUnicode();
    rebuild();
}

void EmojiPickerModel::loadUnicode()
{
    QFile file(QStringLiteral(":/data/emoji.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Emoji data missing");
        return;
    }
    const QJsonArray rows = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("emoji")).toArray();
    m_unicode.reserve(size_t(rows.size()));
    for (const QJsonValue& value : rows) {
        const QJsonArray row = value.toArray();
        Entry entry;
        entry.text = row.at(0).toString();
        entry.category = row.at(1).toString();
        entry.name = row.at(2).toString();
        m_unicode.push_back(entry);
    }
}

void EmojiPickerModel::reloadServerEmoji()
{
    m_serverEmoji.clear();
    DiscordInstance* instance = GetDiscordInstance();
    Guild* guild = instance ? instance->GetCurrentGuild() : nullptr;
    if (guild && guild->m_snowflake != 0) {
        for (const auto& [id, emoji] : guild->m_emoji) {
            if (!emoji.m_bAvailable)
                continue;
            Entry entry;
            entry.custom = true;
            entry.id = quint64(emoji.m_id);
            entry.name = QString::fromStdString(emoji.m_name);
            entry.animated = emoji.m_bAnimated;
            entry.category = QStringLiteral("server");
            m_serverEmoji.push_back(entry);
        }
        std::sort(m_serverEmoji.begin(), m_serverEmoji.end(), [](const Entry& a, const Entry& b) {
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });
    }
    emit serverEmojiChanged();
    if (m_category == QLatin1String("server") && m_serverEmoji.empty())
        setCategory(QStringLiteral("faces"));
    else
        rebuild();
}

void EmojiPickerModel::setCategory(const QString& category)
{
    if (m_category == category)
        return;
    m_category = category;
    emit categoryChanged();
    rebuild();
}

void EmojiPickerModel::setSearch(const QString& search)
{
    const QString trimmed = search.trimmed();
    if (m_search == trimmed)
        return;
    m_search = trimmed;
    emit searchChanged();
    rebuild();
}

QVariantList EmojiPickerModel::categories() const
{
    QVariantList list;
    for (const Category& category : Categories) {
        list.append(QVariantMap{
            {QStringLiteral("key"), QString::fromLatin1(category.key)},
            {QStringLiteral("label"), QString::fromLatin1(category.label)},
            {QStringLiteral("icon"), QString::fromUtf8(category.icon)},
        });
    }
    return list;
}

void EmojiPickerModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    if (!m_search.isEmpty()) {
        // Custom emoji first, like Discord.
        for (const Entry& entry : m_serverEmoji) {
            if (entry.name.contains(m_search, Qt::CaseInsensitive))
                m_rows.push_back(&entry);
        }
        for (const Entry& entry : m_unicode) {
            if (entry.name.contains(m_search, Qt::CaseInsensitive))
                m_rows.push_back(&entry);
        }
    } else if (m_category == QLatin1String("server")) {
        for (const Entry& entry : m_serverEmoji)
            m_rows.push_back(&entry);
    } else {
        for (const Entry& entry : m_unicode) {
            if (entry.category == m_category)
                m_rows.push_back(&entry);
        }
    }
    endResetModel();
    emit countChanged();
}

int EmojiPickerModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant EmojiPickerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= int(m_rows.size()))
        return QVariant();
    const Entry& entry = *m_rows[size_t(index.row())];

    switch (role) {
    case IsCustomRole: return entry.custom;
    case TextRole:     return entry.text;
    case NameRole:     return entry.name;
    case ImageUrlRole:
        return entry.custom ? DiscordUrls::emoji(Snowflake(entry.id), false) : QString();
    case ReactionRole:
        return entry.custom ? QStringLiteral("%1:%2").arg(entry.name).arg(entry.id) : entry.text;
    case InsertTextRole:
        if (!entry.custom)
            return entry.text;
        return QLatin1Char(':') + entry.name + QLatin1Char(':');
    }
    return QVariant();
}

QHash<int, QByteArray> EmojiPickerModel::roleNames() const
{
    return {
        {IsCustomRole, "isCustom"},
        {TextRole, "text"},
        {ImageUrlRole, "imageUrl"},
        {NameRole, "name"},
        {ReactionRole, "reaction"},
        {InsertTextRole, "insertText"},
    };
}
