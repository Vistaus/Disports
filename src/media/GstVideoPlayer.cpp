#include "GstVideoPlayer.h"

#include <QMutexLocker>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "Log.h"

namespace {

constexpr int BusPollMs = 40;

GstFlowReturn newSample(GstAppSink* appsink, gpointer player)
{
    GstVideoPlayer::onNewSample(appsink, player);
    return GST_FLOW_OK;
}

// Android hardware decoders through libhybris (amcviddec-*), or any decoder
// that says it is hardware.
bool isHardwareVideoDecoder(GstElementFactory* factory)
{
    const gchar* klass = gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_KLASS);
    if (!klass || !g_strrstr(klass, "Decoder") || !g_strrstr(klass, "Video"))
        return false;
    return g_str_has_prefix(GST_OBJECT_NAME(factory), "amc") || g_strrstr(klass, "Hardware");
}

// decodebin's autoplug-select, in software mode: skip hardware decoders.
// (force-sw-decoders only knows decoders whose class says "Hardware"; the
// hybris ones do not.)
int autoplugSelect(GstElement*, GstPad*, GstCaps*, GstElementFactory* factory, gpointer)
{
    enum { Try = 0, Skip = 2 }; // GstAutoplugSelectResult
    return isHardwareVideoDecoder(factory) ? Skip : Try;
}

// playbin's element-setup: every element it or its sub-bins create.
void elementSetup(GstElement*, GstElement* element, gpointer player)
{
    GstVideoPlayer::onElementAdded(element, player);
}


}

GstVideoPlayer::GstVideoPlayer(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    m_busTimer.setInterval(BusPollMs);
    connect(&m_busTimer, &QTimer::timeout, this, &GstVideoPlayer::pollBus);
}

GstVideoPlayer::~GstVideoPlayer()
{
    destroyPipeline();
}

bool GstVideoPlayer::isAvailable()
{
    static const bool available = [] {
        if (!gst_init_check(nullptr, nullptr, nullptr))
            return false;
        for (const char* name : {"playbin", "appsink"}) {
            GstElementFactory* factory = gst_element_factory_find(name);
            if (!factory)
                return false;
            gst_object_unref(factory);
        }
        return true;
    }();
    return available;
}

void GstVideoPlayer::setSource(const QUrl& source)
{
    if (m_source == source)
        return;
    m_source = source;
    emit sourceChanged();

    destroyPipeline();
    m_hardwareFailed = false;
    {
        QMutexLocker lock(&m_frameLock);
        m_frame = QImage();
        m_newFrame = false;
        m_clearFrame = true;
    }
    update();
    if (m_hasFrame) {
        m_hasFrame = false;
        emit hasFrameChanged();
    }
    setError(QString());
    if (isComponentComplete() && !m_source.isEmpty() && (m_autoPlay || m_wantPlaying))
        play();
}

void GstVideoPlayer::setAutoPlay(bool autoPlay)
{
    if (m_autoPlay == autoPlay)
        return;
    m_autoPlay = autoPlay;
    emit autoPlayChanged();
    if (isComponentComplete() && m_autoPlay && !m_source.isEmpty() && !m_pipeline)
        play();
}

void GstVideoPlayer::componentComplete()
{
    QQuickItem::componentComplete();
    // Only now, with every property set: loops decides the decoders.
    if (m_autoPlay && !m_source.isEmpty() && !m_pipeline)
        play();
}

void GstVideoPlayer::setLoops(bool loops)
{
    if (m_loops == loops)
        return;
    m_loops = loops;
    emit loopsChanged();
    // Looping picks other decoders: start over with them.
    if (m_pipeline && m_softwareDecoding != (m_loops || m_hardwareFailed)) {
        const bool wasPlaying = m_wantPlaying;
        destroyPipeline();
        if (wasPlaying)
            play();
    }
}

void GstVideoPlayer::setMuted(bool muted)
{
    if (m_muted == muted)
        return;
    m_muted = muted;
    if (m_pipeline)
        g_object_set(m_pipeline, "mute", gboolean(m_muted), nullptr);
    emit mutedChanged();
}

void GstVideoPlayer::setFillMode(FillMode mode)
{
    if (m_fillMode == mode)
        return;
    m_fillMode = mode;
    emit fillModeChanged();
    update();
}

void GstVideoPlayer::play()
{
    m_wantPlaying = true;
    if (m_source.isEmpty())
        return;
    if (!m_pipeline)
        createPipeline();
    if (!m_pipeline)
        return;
    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
        setError(tr("The video could not be played."));
    else
        setPlaying(true);
}

void GstVideoPlayer::pause()
{
    m_wantPlaying = false;
    if (m_pipeline)
        gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    setPlaying(false);
}

void GstVideoPlayer::stop()
{
    m_wantPlaying = false;
    destroyPipeline();
}

void GstVideoPlayer::createPipeline()
{
    if (!isAvailable())
        return setError(tr("Videos can't be played on this device."));

    m_pipeline = gst_element_factory_make("playbin", nullptr);
    GstElement* appsink = gst_element_factory_make("appsink", nullptr);
    if (!m_pipeline || !appsink) {
        if (appsink)
            gst_object_unref(appsink);
        if (m_pipeline)
            gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        return setError(tr("Videos can't be played on this device."));
    }

    // playbin converts to what the texture takes directly.
    GstCaps* caps = gst_caps_from_string("video/x-raw, format=(string)RGBx");
    g_object_set(appsink,
                 "caps", caps,
                 "max-buffers", 2,
                 "drop", TRUE,
                 "sync", TRUE,
                 nullptr);
    gst_caps_unref(caps);

    GstAppSinkCallbacks callbacks = {};
    callbacks.new_sample = &newSample;
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, nullptr);

    m_hardwareDecoder = false;
    m_softwareDecoding = m_loops || m_hardwareFailed;
    g_signal_connect(m_pipeline, "element-setup", G_CALLBACK(elementSetup), this);

    const QByteArray uri = m_source.toEncoded();
    g_object_set(m_pipeline,
                 "uri", uri.constData(),
                 "video-sink", appsink, // playbin takes the reference
                 "mute", gboolean(m_muted),
                 nullptr);
    m_busTimer.start();
}

void GstVideoPlayer::destroyPipeline()
{
    m_busTimer.stop();
    if (m_pipeline) {
        // Stops and joins the streaming threads, so no more frames arrive.
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
    setPlaying(false);
}

void GstVideoPlayer::onNewSample(GstAppSink* appsink, void* player)
{
    auto* self = static_cast<GstVideoPlayer*>(player);
    GstSample* sample = gst_app_sink_pull_sample(appsink);
    if (!sample)
        return;

    GstVideoInfo info;
    GstVideoFrame mapped;
    if (gst_video_info_from_caps(&info, gst_sample_get_caps(sample))
            && gst_video_frame_map(&mapped, &info, gst_sample_get_buffer(sample), GST_MAP_READ)) {
        // A deep copy: the buffer goes back to GStreamer.
        const QImage frame = QImage(static_cast<const uchar*>(GST_VIDEO_FRAME_PLANE_DATA(&mapped, 0)),
                                    GST_VIDEO_INFO_WIDTH(&info), GST_VIDEO_INFO_HEIGHT(&info),
                                    GST_VIDEO_FRAME_PLANE_STRIDE(&mapped, 0), QImage::Format_RGBX8888)
                                 .copy();
        gst_video_frame_unmap(&mapped);
        {
            QMutexLocker lock(&self->m_frameLock);
            self->m_frame = frame;
            self->m_newFrame = true;
        }
        if (!self->m_updateQueued.exchange(true))
            QMetaObject::invokeMethod(self, &GstVideoPlayer::frameArrived, Qt::QueuedConnection);
    }
    gst_sample_unref(sample);
}

void GstVideoPlayer::onElementAdded(GstElement* element, void* player)
{
    auto* self = static_cast<GstVideoPlayer*>(player);
    // uridecodebin and decodebin ask before they pick a decoder.
    if (self->m_softwareDecoding && g_signal_lookup("autoplug-select", G_OBJECT_TYPE(element)))
        g_signal_connect(element, "autoplug-select", G_CALLBACK(autoplugSelect), nullptr);
    GstElementFactory* factory = gst_element_get_factory(element);
    if (!factory)
        return;
    const gchar* klass = gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_KLASS);
    if (klass && g_strrstr(klass, "Decoder") && g_strrstr(klass, "Video"))
        qCDebug(lcVideo, "decoding with %s", GST_OBJECT_NAME(factory));
    if (isHardwareVideoDecoder(factory))
        self->m_hardwareDecoder = true;
}

bool GstVideoPlayer::retryInSoftware()
{
    if (m_softwareDecoding || !m_hardwareDecoder)
        return false;
    qCWarning(lcVideo, "hardware decoder failed on %s, retrying with software decoders",
             qPrintable(m_source.toString()));
    m_hardwareFailed = true;
    // Not from inside pollBus(), which is still using the pipeline.
    QTimer::singleShot(0, this, [this]() {
        destroyPipeline();
        if (m_wantPlaying || m_autoPlay)
            play();
    });
    return true;
}

void GstVideoPlayer::frameArrived()
{
    m_updateQueued = false;
    if (!m_pipeline)
        return;
    update();
    if (!m_hasFrame) {
        m_hasFrame = true;
        emit hasFrameChanged();
    }
}

QSGNode* GstVideoPlayer::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<QSGSimpleTextureNode*>(oldNode);
    QImage frame;
    {
        QMutexLocker lock(&m_frameLock);
        if (m_clearFrame) {
            m_clearFrame = false;
            delete node;
            node = nullptr;
        }
        if (m_newFrame) {
            frame = m_frame;
            m_newFrame = false;
        }
    }

    if (!frame.isNull()) {
        if (!node) {
            node = new QSGSimpleTextureNode;
            node->setOwnsTexture(true);
            node->setFiltering(QSGTexture::Linear);
        }
        node->setTexture(window()->createTextureFromImage(frame));
    }
    if (!node || !node->texture())
        return node;

    // Place the frame by the fill mode.
    const QSizeF frameSize = node->texture()->textureSize();
    const QRectF bounds = boundingRect();
    QRectF target = bounds;
    QRectF source(QPointF(0, 0), frameSize);
    if (m_fillMode != Stretch && !frameSize.isEmpty() && !bounds.isEmpty()) {
        const qreal scaleFit = qMin(bounds.width() / frameSize.width(), bounds.height() / frameSize.height());
        const qreal scaleCrop = qMax(bounds.width() / frameSize.width(), bounds.height() / frameSize.height());
        if (m_fillMode == PreserveAspectFit) {
            const QSizeF size = frameSize * scaleFit;
            target = QRectF(bounds.center() - QPointF(size.width() / 2, size.height() / 2), size);
        } else {
            const QSizeF visible(bounds.width() / scaleCrop, bounds.height() / scaleCrop);
            source = QRectF(QPointF((frameSize.width() - visible.width()) / 2,
                                    (frameSize.height() - visible.height()) / 2),
                            visible);
        }
    }
    node->setRect(target);
    node->setSourceRect(source);
    return node;
}

void GstVideoPlayer::pollBus()
{
    if (!m_pipeline)
        return;
    GstBus* bus = gst_element_get_bus(m_pipeline);
    while (GstMessage* message = gst_bus_pop(bus)) {
        switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            if (retryInSoftware()) {
                gst_message_unref(message);
                gst_object_unref(bus);
                return;
            }
            GError* error = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(message, &error, &debug);
            qCWarning(lcVideo, "%s (%s)", error ? error->message : "?", debug ? debug : "");
            setError(error ? QString::fromUtf8(error->message) : tr("The video could not be played."));
            g_clear_error(&error);
            g_free(debug);
            gst_element_set_state(m_pipeline, GST_STATE_READY);
            setPlaying(false);
            break;
        }
        case GST_MESSAGE_EOS: {
            // A hardware decoder that gives up ends the stream early.
            gint64 position = -1, duration = -1;
            gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &position);
            gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &duration);
            const bool early = duration > 0 && position >= 0 && position < duration - 300 * GST_MSECOND;
            if (early && retryInSoftware()) {
                gst_message_unref(message);
                gst_object_unref(bus);
                return;
            }
            if (m_loops && m_wantPlaying) {
                gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME,
                                        GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 0);
            } else {
                // Rewind, so play() starts over.
                m_wantPlaying = false;
                gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
                gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME,
                                        GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 0);
                setPlaying(false);
            }
            break;
        }
        case GST_MESSAGE_BUFFERING: {
            // Network streams: wait until enough is buffered.
            gint percent = 100;
            gst_message_parse_buffering(message, &percent);
            if (m_wantPlaying)
                gst_element_set_state(m_pipeline, percent < 100 ? GST_STATE_PAUSED : GST_STATE_PLAYING);
            break;
        }
        default:
            break;
        }
        gst_message_unref(message);
        if (!m_pipeline)
            break;
    }
    gst_object_unref(bus);
}

void GstVideoPlayer::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    emit playingChanged();
}

void GstVideoPlayer::setError(const QString& error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
