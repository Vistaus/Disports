#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>
#include <QTimer>

#include <nlohmann/json.hpp>

#include "discord/models/ScrollDir.hpp"
#include "discord/models/Snowflake.hpp"

// What the app shows before the gateway connects: the last READY and
// READY_SUPPLEMENTAL, and the newest messages of channels that were opened.
// Kept as Discord's raw JSON, so the core reads it as if it came from
// Discord. Stored compressed in the cache folder, written on a worker
// thread.
class OfflineCache : public QObject
{
    Q_OBJECT

public:
    static constexpr int MessagesPerChannel = 50;

    explicit OfflineCache(QObject* parent = nullptr);
    ~OfflineCache() override;

    // The saved READY (and READY_SUPPLEMENTAL, if there is one). False when
    // there is none.
    bool loadReady(nlohmann::json& ready, nlohmann::json& supplemental);

    // The newest cached messages of a channel (newest first), or an empty
    // array.
    nlohmann::json messages(Snowflake channel);

    // Fed by the core, see Frontend::OnGatewayDispatch / OnMessagesFetched.
    void gatewayDispatch(const std::string& type, const nlohmann::json& message);
    void messagesFetched(Snowflake channel, ScrollDir::eScrollDir sd, Snowflake anchor, const nlohmann::json& messages);

    // Forgets everything (logging out).
    void clear();

private:
    QString path(const QString& name) const;
    QString channelPath(Snowflake channel) const;
    // The channel's messages, loaded from disk the first time; null when the
    // channel is not cached.
    nlohmann::json* channel(Snowflake channel);
    void markDirty(Snowflake channel);
    void flush();
    void writeLater(const QString& path, const nlohmann::json& value);

    QString m_dir;
    QHash<Snowflake, nlohmann::json> m_channels; // loaded ones
    QSet<Snowflake> m_missing;                   // known not to be cached
    QSet<Snowflake> m_dirty;
    QTimer m_flushTimer;
    QThreadPool m_writer; // one thread: writes happen in order
};
