#pragma once

#include <QImage>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QTimer>
#include <QUrl>

#include <memory>

class QNetworkReply;
namespace rlottie {
class Animation;
}

// A Lottie animation (Discord's Lottie stickers) drawn with rlottie. Shows
// the first frame until `playing`; then loops, at most 30 frames a second.
class LottieView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    // The animation could be fetched and read.
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
    explicit LottieView(QQuickItem* parent = nullptr);
    ~LottieView() override;

    QUrl source() const { return m_source; }
    void setSource(const QUrl& source);
    bool playing() const { return m_playing; }
    void setPlaying(bool playing);
    bool ready() const { return m_animation != nullptr; }

    void paint(QPainter* painter) override;

signals:
    void sourceChanged();
    void playingChanged();
    void readyChanged();

protected:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void load();
    void loaded();
    void render();
    void updateTimer();

    QUrl m_source;
    bool m_playing = false;
    QPointer<QNetworkReply> m_reply;
    std::unique_ptr<rlottie::Animation> m_animation;
    QImage m_frame;
    size_t m_frameNumber = 0;
    QTimer m_timer;
};
