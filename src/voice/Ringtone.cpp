#include "Ringtone.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#include <gst/gst.h>

#include "discord/config/DiscordClientConfig.hpp"
#include "discord/network/DiscordAPI.hpp"

namespace {

// The web client's call_ringing.mp3. Its name is a hash of the file, so it
// only changes when Discord changes the sound.
const char DiscordRingtone[] = "c2a7111bb44b8da0.mp3";

QString cachedPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/sounds/") + QLatin1String(DiscordRingtone);
}

// Written when a download fails, so the next try waits a day.
QString failedPath()
{
    return cachedPath() + QStringLiteral(".failed");
}

QString ringtonePath()
{
    const QString candidates[] = {
        cachedPath(),
        QStringLiteral("/usr/share/sounds/lomiri/ringtones/UBports.ogg"),
        QStringLiteral("/usr/share/sounds/lomiri/ringtones/Ubuntu.ogg"),
        QStringLiteral("/usr/share/sounds/ubuntu/ringtones/Ubuntu.ogg"),
        QStringLiteral("/usr/share/sounds/freedesktop/stereo/phone-incoming-call.oga"),
    };
    for (const QString& path : candidates) {
        if (QFile::exists(path))
            return path;
    }
    return QString();
}

}

Ringtone::Ringtone(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent)
    , m_nam(nam)
{
    m_busTimer.setInterval(250);
    connect(&m_busTimer, &QTimer::timeout, this, &Ringtone::poll);
}

Ringtone::~Ringtone()
{
    stop();
    if (m_reply)
        m_reply->abort();
}

void Ringtone::fetch()
{
    if (m_reply || QFile::exists(cachedPath()))
        return;
    const QFileInfo failed(failedPath());
    if (failed.exists() && failed.lastModified().secsTo(QDateTime::currentDateTime()) < 24 * 3600)
        return;
    // From the same site as the API: discord.com, or the test server.
    const QUrl api(QString::fromStdString(GetDiscordAPI()));
    QNetworkRequest request(api.resolved(QUrl(QStringLiteral("/assets/") + QLatin1String(DiscordRingtone))));
    request.setRawHeader("User-Agent", QByteArray::fromStdString(GetClientConfig()->GetUserAgent()));
    m_reply = m_nam->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &Ringtone::downloaded);
}

void Ringtone::downloaded()
{
    QNetworkReply* reply = m_reply;
    if (!reply)
        return;
    reply->deleteLater();
    m_reply = nullptr;
    const QByteArray data = reply->readAll();
    const bool ok = reply->error() == QNetworkReply::NoError
        && reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith(QLatin1String("audio/"))
        && data.size() > 1024 && data.size() < 4 * 1024 * 1024;
    QDir().mkpath(QFileInfo(cachedPath()).path());
    QSaveFile file(ok ? cachedPath() : failedPath());
    if (file.open(QIODevice::WriteOnly)) {
        if (ok)
            file.write(data);
        file.commit();
    }
    if (ok)
        QFile::remove(failedPath());
}

void Ringtone::play()
{
    if (m_pipeline)
        return;
    const QString path = ringtonePath();
    if (path.isEmpty())
        return;
    if (!gst_is_initialized())
        gst_init(nullptr, nullptr);
    m_pipeline = gst_element_factory_make("playbin", "ringtone");
    if (!m_pipeline)
        return;
    // Rings like a phone call does (volume, routing).
    if (GstElement* sink = gst_element_factory_make("pulsesink", nullptr)) {
        GstStructure* props = gst_structure_new("props", "media.role", G_TYPE_STRING, "phone", nullptr);
        g_object_set(sink, "stream-properties", props, nullptr);
        gst_structure_free(props);
        g_object_set(m_pipeline, "audio-sink", sink, nullptr);
    }
    const QByteArray uri = QUrl::fromLocalFile(path).toEncoded();
    g_object_set(m_pipeline, "uri", uri.constData(), nullptr);
    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    m_busTimer.start();
}

void Ringtone::stop()
{
    m_busTimer.stop();
    if (!m_pipeline)
        return;
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_object_unref(m_pipeline);
    m_pipeline = nullptr;
}

void Ringtone::poll()
{
    GstBus* bus = gst_element_get_bus(m_pipeline);
    while (GstMessage* message = gst_bus_pop_filtered(bus, GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR))) {
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
            // Again, after a short pause.
            gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, 0);
        } else {
            gst_message_unref(message);
            gst_object_unref(bus);
            stop();
            // A broken download: drop it and ring with the system's.
            if (QFile::exists(cachedPath())) {
                QFile::remove(cachedPath());
                QFile marker(failedPath());
                if (marker.open(QIODevice::WriteOnly))
                    marker.close();
                play();
            }
            return;
        }
        gst_message_unref(message);
    }
    gst_object_unref(bus);
}
