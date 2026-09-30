#include "Ringtone.h"

#include <QFile>
#include <QUrl>

#include <gst/gst.h>

namespace {

QString ringtonePath()
{
    const QString candidates[] = {
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

Ringtone::Ringtone(QObject* parent)
    : QObject(parent)
{
    m_busTimer.setInterval(250);
    connect(&m_busTimer, &QTimer::timeout, this, &Ringtone::poll);
}

Ringtone::~Ringtone()
{
    stop();
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
            return;
        }
        gst_message_unref(message);
    }
    gst_object_unref(bus);
}
