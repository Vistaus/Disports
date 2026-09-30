#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickItem>
#include <QTimer>
#include <QUrl>

#include <atomic>

typedef struct _GstElement GstElement;
typedef struct _GstAppSink GstAppSink;

// A QML item that plays a video with GStreamer and draws the frames as its
// texture. QtMultimedia has no backend on Ubuntu Touch's Qt 6.
//
// Videos use the phone's hardware decoders, and are played again with
// software decoders if those fail. Looping videos (GIFs) always use software
// decoders: the hardware ones can't restart at the end of a stream.
class GstVideoPlayer : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool autoPlay READ autoPlay WRITE setAutoPlay NOTIFY autoPlayChanged)
    Q_PROPERTY(bool loops READ loops WRITE setLoops NOTIFY loopsChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(FillMode fillMode READ fillMode WRITE setFillMode NOTIFY fillModeChanged)
    // Playing, or about to (loading, looping, buffering); false when paused,
    // finished or failed.
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    // A frame has been shown since the source was set.
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)

public:
    enum FillMode { Stretch, PreserveAspectFit, PreserveAspectCrop };
    Q_ENUM(FillMode)

    explicit GstVideoPlayer(QQuickItem* parent = nullptr);
    ~GstVideoPlayer() override;

    // GStreamer initialises and has playbin and appsink.
    static bool isAvailable();

    QUrl source() const { return m_source; }
    void setSource(const QUrl& source);
    bool autoPlay() const { return m_autoPlay; }
    void setAutoPlay(bool autoPlay);
    bool loops() const { return m_loops; }
    void setLoops(bool loops);
    bool muted() const { return m_muted; }
    void setMuted(bool muted);
    FillMode fillMode() const { return m_fillMode; }
    void setFillMode(FillMode mode);
    bool playing() const { return m_playing; }
    bool hasFrame() const { return m_hasFrame; }
    QString errorString() const { return m_error; }

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();

    // Called by the appsink on its streaming thread when a frame is ready.
    static void onNewSample(GstAppSink* appsink, void* player);
    // Called by playbin when it sets up an element, from any thread.
    static void onElementAdded(GstElement* element, void* player);

signals:
    void sourceChanged();
    void autoPlayChanged();
    void loopsChanged();
    void mutedChanged();
    void fillModeChanged();
    void playingChanged();
    void hasFrameChanged();
    void errorChanged();

protected:
    void componentComplete() override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    void createPipeline();
    void destroyPipeline();
    void pollBus();
    void frameArrived();
    void setPlaying(bool playing);
    void setError(const QString& error);
    // Restarts in software after the hardware decoder failed; false when
    // software was already used.
    bool retryInSoftware();

    QUrl m_source;
    bool m_autoPlay = false;
    bool m_loops = false;
    bool m_muted = false;
    FillMode m_fillMode = PreserveAspectFit;
    bool m_playing = false;
    bool m_hasFrame = false;
    bool m_wantPlaying = false;
    bool m_softwareDecoding = false; // for the current pipeline
    bool m_hardwareFailed = false;   // on the current source
    std::atomic_bool m_hardwareDecoder{false}; // playbin picked one
    QString m_error;

    GstElement* m_pipeline = nullptr;
    QTimer m_busTimer;

    // The newest frame, written by the streaming thread and taken by the
    // render thread. Frames that arrive before the last one was drawn
    // replace it, so a slow GUI never queues them up.
    QMutex m_frameLock;
    QImage m_frame;
    bool m_newFrame = false;
    bool m_clearFrame = false; // drop the shown frame (new source)
    std::atomic_bool m_updateQueued{false};
};
