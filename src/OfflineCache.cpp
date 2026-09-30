#include "OfflineCache.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <memory>

namespace {

constexpr int FlushDelayMs = 3000;

// Bumped when the files change in a way old ones cannot be read.
const QString FormatVersion = QStringLiteral("1");

Snowflake snowflake(const nlohmann::json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key))
        return 0;
    const auto& value = object[key];
    if (value.is_string())
        return Snowflake(QByteArray::fromStdString(value.get<std::string>()).toULongLong());
    if (value.is_number_unsigned())
        return value.get<Snowflake>();
    return 0;
}

bool readJson(const QString& path, nlohmann::json& out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = qUncompress(file.readAll());
    if (data.isEmpty())
        return false;
    out = nlohmann::json::parse(data.constBegin(), data.constEnd(), nullptr, false);
    return !out.is_discarded();
}

void writeJson(const QString& path, const nlohmann::json& value)
{
    const std::string text = value.dump();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(qCompress(QByteArray::fromRawData(text.data(), qsizetype(text.size()))));
    file.commit();
}

// MESSAGE_UPDATE may carry only the fields that changed (embeds resolving,
// for one); lay them over the cached message.
void mergeInto(nlohmann::json& message, const nlohmann::json& update)
{
    for (auto it = update.begin(); it != update.end(); ++it)
        message[it.key()] = it.value();
}

}

OfflineCache::OfflineCache(QObject* parent)
    : QObject(parent)
    , m_dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/offline"))
{
    m_writer.setMaxThreadCount(1);
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(FlushDelayMs);
    connect(&m_flushTimer, &QTimer::timeout, this, &OfflineCache::flush);

    QDir dir(m_dir);
    if (QFile::exists(path(QStringLiteral("version")))) {
        QFile version(path(QStringLiteral("version")));
        if (version.open(QIODevice::ReadOnly) && QString::fromLatin1(version.readAll()) != FormatVersion)
            dir.removeRecursively();
    }
    dir.mkpath(QStringLiteral("channels"));
    QFile version(path(QStringLiteral("version")));
    if (version.open(QIODevice::WriteOnly))
        version.write(FormatVersion.toLatin1());
}

OfflineCache::~OfflineCache()
{
    flush();
    m_writer.waitForDone();
}

QString OfflineCache::path(const QString& name) const
{
    return m_dir + QLatin1Char('/') + name;
}

QString OfflineCache::channelPath(Snowflake channel) const
{
    return path(QStringLiteral("channels/%1.json.z").arg(quint64(channel)));
}

bool OfflineCache::loadReady(nlohmann::json& ready, nlohmann::json& supplemental)
{
    if (!readJson(path(QStringLiteral("ready.json.z")), ready) || !ready.contains("d"))
        return false;
    if (!readJson(path(QStringLiteral("ready_supplemental.json.z")), supplemental))
        supplemental = nlohmann::json();
    return true;
}

nlohmann::json OfflineCache::messages(Snowflake id)
{
    nlohmann::json* cached = channel(id);
    return cached ? *cached : nlohmann::json::array();
}

nlohmann::json* OfflineCache::channel(Snowflake id)
{
    auto it = m_channels.find(id);
    if (it != m_channels.end())
        return &it.value();
    if (m_missing.contains(id))
        return nullptr;
    nlohmann::json loaded;
    if (!readJson(channelPath(id), loaded) || !loaded.is_array()) {
        m_missing.insert(id);
        return nullptr;
    }
    return &m_channels.insert(id, std::move(loaded)).value();
}

void OfflineCache::gatewayDispatch(const std::string& type, const nlohmann::json& message)
{
    if (type == "READY") {
        writeLater(path(QStringLiteral("ready.json.z")), message);
        return;
    }
    if (type == "READY_SUPPLEMENTAL") {
        writeLater(path(QStringLiteral("ready_supplemental.json.z")), message);
        return;
    }
    if (type.rfind("MESSAGE_", 0) != 0 || !message.contains("d"))
        return;

    const nlohmann::json& data = message["d"];
    const Snowflake channelId = snowflake(data, "channel_id");
    // Only channels that were opened are cached; others are fetched fresh.
    nlohmann::json* messages = channelId ? channel(channelId) : nullptr;
    if (!messages)
        return;

    const Snowflake messageId = snowflake(data, "id");
    auto find = [&](Snowflake id) {
        return std::find_if(messages->begin(), messages->end(), [id](const nlohmann::json& m) {
            return snowflake(m, "id") == id;
        });
    };

    if (type == "MESSAGE_CREATE") {
        if (find(messageId) != messages->end())
            return;
        messages->insert(messages->begin(), data);
        if (messages->size() > size_t(MessagesPerChannel))
            messages->erase(messages->begin() + MessagesPerChannel, messages->end());
    } else if (type == "MESSAGE_UPDATE") {
        auto it = find(messageId);
        if (it == messages->end())
            return;
        mergeInto(*it, data);
    } else if (type == "MESSAGE_DELETE") {
        auto it = find(messageId);
        if (it == messages->end())
            return;
        messages->erase(it);
    } else if (type == "MESSAGE_DELETE_BULK") {
        if (!data.contains("ids") || !data["ids"].is_array())
            return;
        QSet<Snowflake> ids;
        for (const auto& id : data["ids"])
            if (id.is_string())
                ids.insert(Snowflake(QByteArray::fromStdString(id.get<std::string>()).toULongLong()));
        messages->erase(std::remove_if(messages->begin(), messages->end(), [&](const nlohmann::json& m) {
                            return ids.contains(snowflake(m, "id"));
                        }),
                        messages->end());
    } else {
        // Reactions and the like: the next fetch brings them.
        return;
    }
    markDirty(channelId);
}

void OfflineCache::messagesFetched(Snowflake id, ScrollDir::eScrollDir sd, Snowflake anchor,
                                   const nlohmann::json& fetched)
{
    // Only the newest messages of a channel; older pages are not kept.
    if (sd != ScrollDir::BEFORE || anchor != 0 || !fetched.is_array())
        return;
    nlohmann::json newest = nlohmann::json::array();
    for (size_t i = 0; i < fetched.size() && i < size_t(MessagesPerChannel); ++i)
        newest.push_back(fetched[i]);
    m_missing.remove(id);
    m_channels.insert(id, std::move(newest));
    markDirty(id);
}

void OfflineCache::markDirty(Snowflake id)
{
    m_dirty.insert(id);
    if (!m_flushTimer.isActive())
        m_flushTimer.start();
}

void OfflineCache::flush()
{
    m_flushTimer.stop();
    for (Snowflake id : std::as_const(m_dirty)) {
        auto it = m_channels.constFind(id);
        if (it != m_channels.constEnd())
            writeLater(channelPath(id), it.value());
    }
    m_dirty.clear();
}

void OfflineCache::writeLater(const QString& target, const nlohmann::json& value)
{
    // The copy is made here; dumping, compressing and writing happen on the
    // writer thread.
    auto copy = std::make_shared<nlohmann::json>(value);
    m_writer.start([target, copy]() { writeJson(target, *copy); });
}

void OfflineCache::clear()
{
    m_flushTimer.stop();
    m_dirty.clear();
    m_channels.clear();
    m_missing.clear();
    m_writer.waitForDone();
    QDir(m_dir).removeRecursively();
    QDir().mkpath(m_dir + QStringLiteral("/channels"));
    QFile version(path(QStringLiteral("version")));
    if (version.open(QIODevice::WriteOnly))
        version.write(FormatVersion.toLatin1());
}
