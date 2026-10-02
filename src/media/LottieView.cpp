#include "LottieView.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QQmlEngine>
#include <QQuickWindow>

#include <rlottie.h>

LottieView::LottieView(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        if (!m_animation)
            return;
        m_frameNumber = (m_frameNumber + 1) % qMax<size_t>(1, m_animation->totalFrame());
        render();
    });
}

LottieView::~LottieView()
{
    if (m_reply)
        m_reply->abort();
}

void LottieView::setSource(const QUrl& source)
{
    if (m_source == source)
        return;
    m_source = source;
    emit sourceChanged();
    load();
}

void LottieView::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    emit playingChanged();
    updateTimer();
}

// Through the QML engine's network access, which caches on disk.
void LottieView::load()
{
    if (m_reply)
        m_reply->abort();
    const bool wasReady = ready();
    m_animation.reset();
    m_frame = QImage();
    m_frameNumber = 0;
    updateTimer();
    if (wasReady)
        emit readyChanged();
    update();

    QQmlEngine* engine = qmlEngine(this);
    if (m_source.isEmpty() || !engine || !engine->networkAccessManager())
        return;
    m_reply = engine->networkAccessManager()->get(QNetworkRequest(m_source));
    connect(m_reply, &QNetworkReply::finished, this, &LottieView::loaded);
}

void LottieView::loaded()
{
    QNetworkReply* reply = m_reply;
    if (!reply)
        return;
    reply->deleteLater();
    m_reply = nullptr;
    if (reply->error() != QNetworkReply::NoError)
        return;
    // The key names it in rlottie's cache: one per sticker.
    m_animation = rlottie::Animation::loadFromData(reply->readAll().toStdString(), m_source.toString().toStdString());
    if (!m_animation)
        return;
    emit readyChanged();
    render();
    updateTimer();
}

void LottieView::render()
{
    if (!m_animation)
        return;
    const qreal ratio = window() ? window()->devicePixelRatio() : 1.0;
    const QSize size = (boundingRect().size() * ratio).toSize();
    if (size.isEmpty())
        return;
    if (m_frame.size() != size) {
        m_frame = QImage(size, QImage::Format_ARGB32_Premultiplied);
        m_frame.setDevicePixelRatio(ratio);
    }
    m_frame.fill(Qt::transparent);
    rlottie::Surface surface(reinterpret_cast<uint32_t*>(m_frame.bits()), size_t(size.width()),
                             size_t(size.height()), size_t(m_frame.bytesPerLine()));
    m_animation->renderSync(m_frameNumber, surface);
    update();
}

void LottieView::updateTimer()
{
    if (m_playing && m_animation && m_animation->totalFrame() > 1) {
        const double rate = qBound(1.0, m_animation->frameRate(), 30.0);
        m_timer.start(int(1000.0 / rate));
    } else {
        m_timer.stop();
    }
}

void LottieView::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        render();
}

void LottieView::paint(QPainter* painter)
{
    if (!m_frame.isNull())
        painter->drawImage(QPointF(0, 0), m_frame);
}
