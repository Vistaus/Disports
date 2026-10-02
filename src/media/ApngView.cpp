#include "ApngView.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QQmlEngine>
#include <QtEndian>

namespace {

const QByteArray Signature("\x89PNG\r\n\x1a\n", 8);

quint32 crc32(const QByteArray& data)
{
    static quint32 table[256] = {};
    if (!table[1]) {
        for (quint32 n = 0; n < 256; ++n) {
            quint32 c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
    }
    quint32 crc = 0xFFFFFFFFu;
    for (char byte : data)
        crc = table[(crc ^ quint8(byte)) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

QByteArray chunk(const QByteArray& type, const QByteArray& body)
{
    QByteArray out(4, '\0');
    qToBigEndian<quint32>(quint32(body.size()), out.data());
    const QByteArray typed = type + body;
    out += typed;
    QByteArray crc(4, '\0');
    qToBigEndian<quint32>(crc32(typed), crc.data());
    return out + crc;
}

quint32 be32(const QByteArray& data, int offset)
{
    return qFromBigEndian<quint32>(data.constData() + offset);
}

quint16 be16(const QByteArray& data, int offset)
{
    return qFromBigEndian<quint16>(data.constData() + offset);
}

}

ApngView::ApngView(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ApngView::next);
}

ApngView::~ApngView()
{
    if (m_reply)
        m_reply->abort();
}

void ApngView::setSource(const QUrl& source)
{
    if (m_source == source)
        return;
    m_source = source;
    emit sourceChanged();
    load();
}

void ApngView::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    emit playingChanged();
    if (m_playing && m_frames.size() > 1 && m_current >= 0)
        m_timer.start(m_frames[m_current].delayMs);
    else
        m_timer.stop();
}

// Through the QML engine's network access, which caches on disk.
void ApngView::load()
{
    if (m_reply)
        m_reply->abort();
    const bool wasReady = ready();
    m_timer.stop();
    m_frames.clear();
    m_canvas = m_shown = m_previous = QImage();
    m_current = -1;
    if (wasReady)
        emit readyChanged();
    update();

    QQmlEngine* engine = qmlEngine(this);
    if (m_source.isEmpty() || !engine || !engine->networkAccessManager())
        return;
    m_reply = engine->networkAccessManager()->get(QNetworkRequest(m_source));
    connect(m_reply, &QNetworkReply::finished, this, &ApngView::loaded);
}

void ApngView::loaded()
{
    QNetworkReply* reply = m_reply;
    if (!reply)
        return;
    reply->deleteLater();
    m_reply = nullptr;
    if (reply->error() != QNetworkReply::NoError || !parse(reply->readAll()))
        return;
    showFrame(0);
    if (ready())
        emit readyChanged();
}

// The frames: their fcTL (area, delay, how to dispose and blend) and image
// data (IDAT, or fdAT after the sequence number), each as a PNG of its own
// with the file's other chunks (palette, transparency, colour).
bool ApngView::parse(const QByteArray& data)
{
    if (!data.startsWith(Signature))
        return false;
    QByteArray header;
    QByteArray shared;
    QList<QByteArray> frameData;
    bool animated = false;
    for (qsizetype pos = Signature.size(); pos + 12 <= data.size();) {
        const quint32 length = be32(data, int(pos));
        if (pos + 12 + length > quint64(data.size()))
            return false;
        const QByteArray type = data.mid(pos + 4, 4);
        const QByteArray body = data.mid(pos + 8, length);
        pos += 12 + length;

        if (type == "IHDR" && body.size() == 13) {
            header = body;
        } else if (type == "acTL") {
            animated = true;
        } else if (type == "fcTL" && body.size() == 26) {
            Frame frame;
            frame.area = QRect(int(be32(body, 12)), int(be32(body, 16)), int(be32(body, 4)), int(be32(body, 8)));
            const quint16 numerator = be16(body, 20);
            const quint16 denominator = be16(body, 22) ? be16(body, 22) : 100;
            frame.delayMs = int(1000 * numerator / denominator);
            // As browsers do: a delay this short means "as fast as you like".
            if (frame.delayMs <= 10)
                frame.delayMs = 100;
            frame.dispose = quint8(body[24]);
            frame.blend = quint8(body[25]);
            m_frames.append(frame);
            frameData.append(QByteArray());
        } else if (type == "IDAT") {
            // Before any fcTL, the default image: not part of the animation.
            if (!frameData.isEmpty())
                frameData.last() += body;
        } else if (type == "fdAT" && body.size() > 4) {
            if (!frameData.isEmpty())
                frameData.last() += body.mid(4);
        } else if (type == "IEND") {
            break;
        } else if (frameData.isEmpty()) {
            shared += chunk(type, body);
        }
    }
    if (header.isEmpty())
        return false;

    const QSize size(int(be32(header, 0)), int(be32(header, 4)));
    if (!animated || m_frames.isEmpty()) {
        // A plain PNG.
        m_frames = {Frame{data, QRect(QPoint(0, 0), size), 0, 0, 0}};
    } else {
        for (int i = 0; i < m_frames.size(); ++i) {
            QByteArray frameHeader = header;
            qToBigEndian<quint32>(quint32(m_frames[i].area.width()), frameHeader.data());
            qToBigEndian<quint32>(quint32(m_frames[i].area.height()), frameHeader.data() + 4);
            m_frames[i].png = Signature + chunk("IHDR", frameHeader) + shared + chunk("IDAT", frameData.value(i))
                              + chunk("IEND", QByteArray());
        }
    }
    m_canvas = QImage(size, QImage::Format_ARGB32_Premultiplied);
    return !size.isEmpty();
}

// Frame `index` drawn on the canvas, after the previous one's disposal.
void ApngView::showFrame(int index)
{
    if (index <= 0) {
        m_canvas.fill(Qt::transparent);
    } else {
        const Frame& before = m_frames[index - 1];
        if (before.dispose == 1) {
            QPainter clear(&m_canvas);
            clear.setCompositionMode(QPainter::CompositionMode_Source);
            clear.fillRect(before.area, Qt::transparent);
        } else if (before.dispose == 2 && !m_previous.isNull()) {
            m_canvas = m_previous;
        }
    }
    const Frame& frame = m_frames[index];
    if (frame.dispose == 2)
        m_previous = m_canvas.copy();
    const QImage image = QImage::fromData(frame.png, "PNG");
    if (!image.isNull()) {
        QPainter painter(&m_canvas);
        painter.setCompositionMode(frame.blend == 0 ? QPainter::CompositionMode_Source
                                                    : QPainter::CompositionMode_SourceOver);
        painter.drawImage(frame.area.topLeft(), image);
    }
    m_current = index;
    m_shown = m_canvas;
    update();
    if (m_playing && m_frames.size() > 1)
        m_timer.start(frame.delayMs);
}

void ApngView::next()
{
    if (!m_frames.isEmpty())
        showFrame((m_current + 1) % int(m_frames.size()));
}

void ApngView::paint(QPainter* painter)
{
    if (m_shown.isNull())
        return;
    QSizeF size = m_shown.size();
    size.scale(boundingRect().size(), Qt::KeepAspectRatio);
    const QRectF target(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), size);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(target, m_shown);
}
