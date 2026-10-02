#pragma once

#include <QImage>
#include <QList>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QTimer>
#include <QUrl>

class QNetworkReply;

// An animated PNG (Discord's APNG stickers), which Qt only reads the first
// frame of. Its frames are cut into plain PNGs, decoded one at a time and
// put together as the format says. Shows the first frame until `playing`;
// a PNG that isn't animated just shows.
class ApngView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
    explicit ApngView(QQuickItem* parent = nullptr);
    ~ApngView() override;

    QUrl source() const { return m_source; }
    void setSource(const QUrl& source);
    bool playing() const { return m_playing; }
    void setPlaying(bool playing);
    bool ready() const { return !m_canvas.isNull(); }

    void paint(QPainter* painter) override;

signals:
    void sourceChanged();
    void playingChanged();
    void readyChanged();

private:
    struct Frame {
        QByteArray png; // a plain PNG of this frame's area
        QRect area;     // where it goes on the canvas
        int delayMs = 100;
        quint8 dispose = 0; // 0 none, 1 clear the area, 2 back to before it
        quint8 blend = 0;   // 0 replace, 1 draw over
    };

    void load();
    void loaded();
    bool parse(const QByteArray& data);
    void showFrame(int index);
    void next();

    QUrl m_source;
    bool m_playing = false;
    QPointer<QNetworkReply> m_reply;
    QList<Frame> m_frames;
    QImage m_canvas;   // the animation as built so far
    QImage m_shown;    // what is on screen: m_canvas after the current frame
    QImage m_previous; // the canvas before a frame that goes back to it
    int m_current = -1;
    QTimer m_timer;
};
