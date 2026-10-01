#include "RemoteAuth.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QWebSocket>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include "qrcodegen.hpp"

#include "Captcha.h"
#include "PasswordLogin.h"
#include "backend/QtHttpClient.h"
#include "discord/network/DiscordAPI.hpp"

namespace {
const QString GatewayUrl = QStringLiteral("wss://remote-auth-gateway.discord.gg/?v=2");
const QString LoginUrlPrefix = QStringLiteral("https://discord.com/ra/");

QByteArray urlSafeBase64(const QByteArray& data)
{
    return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}
}

RemoteAuth::RemoteAuth(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent)
    , m_nam(nam)
{
    connect(&m_heartbeat, &QTimer::timeout, this, [this]() {
        send(QJsonObject{{QStringLiteral("op"), QStringLiteral("heartbeat")}});
    });
}

RemoteAuth::~RemoteAuth()
{
    stop();
}

void RemoteAuth::start()
{
    stop();
    m_finished = false;
    m_qrImage.clear();
    emit qrImageChanged();
    setStatus(tr("Preparing QR code..."), true);
    // The remote-auth gateway is Discord's; another server (the test one)
    // has none.
    if (!QString::fromStdString(GetDiscordAPI()).startsWith(QLatin1String("https://discord.com/"))) {
        fail(tr("QR login only works with Discord's own servers."));
        return;
    }

    m_key = EVP_RSA_gen(2048);
    if (!m_key) {
        fail(tr("Discord QR login failed. Tap refresh to try again."));
        return;
    }

    m_socket = new QWebSocket(QStringLiteral("https://discord.com"), QWebSocketProtocol::VersionLatest, this);
    connect(m_socket, &QWebSocket::textMessageReceived, this, &RemoteAuth::handleMessage);
    connect(m_socket, &QWebSocket::disconnected, this, [this]() {
        m_heartbeat.stop();
        if (m_finished)
            return;
        // Never got as far as a code: no connection.
        if (m_qrImage.isEmpty())
            fail(tr("Couldn't reach Discord. Check your internet connection, then tap refresh."));
        else
            fail(tr("The QR code expired. Tap refresh to get a new one."));
    });

    QNetworkRequest request{QUrl(GatewayUrl)};
    QtHttpClient::applyClientHeaders(request);
    m_socket->open(request);
}

void RemoteAuth::stop()
{
    m_finished = true;
    m_heartbeat.stop();
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    if (m_key) {
        EVP_PKEY_free(m_key);
        m_key = nullptr;
    }
}

void RemoteAuth::send(const QJsonObject& object)
{
    if (m_socket)
        m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
}

void RemoteAuth::setStatus(const QString& status, bool busy, bool failed)
{
    m_status = status;
    m_busy = busy;
    m_failed = failed;
    emit statusChanged();
}

void RemoteAuth::fail(const QString& message)
{
    stop();
    setStatus(message, false, true);
}

void RemoteAuth::handleMessage(const QString& text)
{
    const QJsonObject message = QJsonDocument::fromJson(text.toUtf8()).object();
    const QString op = message.value(QStringLiteral("op")).toString();

    if (op == QLatin1String("hello")) {
        const int interval = message.value(QStringLiteral("heartbeat_interval")).toInt(41250);
        m_heartbeat.start(qMax(interval, 1000));

        unsigned char* der = nullptr;
        const int length = i2d_PUBKEY(m_key, &der);
        if (length <= 0) {
            fail(tr("Discord QR login failed. Tap refresh to try again."));
            return;
        }
        const QByteArray publicKey(reinterpret_cast<const char*>(der), length);
        OPENSSL_free(der);

        m_fingerprint = QString::fromLatin1(
            urlSafeBase64(QCryptographicHash::hash(publicKey, QCryptographicHash::Sha256)));
        send(QJsonObject{
            {QStringLiteral("op"), QStringLiteral("init")},
            {QStringLiteral("encoded_public_key"), QString::fromLatin1(publicKey.toBase64())},
        });
    } else if (op == QLatin1String("nonce_proof")) {
        const QByteArray nonce = decrypt(message.value(QStringLiteral("encrypted_nonce")).toString().toLatin1());
        send(QJsonObject{
            {QStringLiteral("op"), QStringLiteral("nonce_proof")},
            {QStringLiteral("nonce"), QString::fromLatin1(urlSafeBase64(nonce))},
        });
    } else if (op == QLatin1String("pending_remote_init")) {
        const QString fingerprint = message.value(QStringLiteral("fingerprint")).toString();
        if (fingerprint != m_fingerprint) {
            fail(tr("Discord QR login failed. Tap refresh to try again."));
            return;
        }
        m_qrImage = renderQr(LoginUrlPrefix + fingerprint);
        emit qrImageChanged();
        setStatus(tr("Open Discord on your Android or iOS device and scan to sign in."), false);
    } else if (op == QLatin1String("pending_ticket")) {
        setStatus(tr("Confirm the login on your phone."), true);
    } else if (op == QLatin1String("pending_login")) {
        completeLogin(message.value(QStringLiteral("ticket")).toString());
    } else if (op == QLatin1String("cancel")) {
        fail(tr("QR login was canceled on your phone."));
    }
}

void RemoteAuth::completeLogin(const QString& ticket)
{
    if (ticket.isEmpty())
        return;
    setStatus(tr("Signing in..."), true);

    QNetworkRequest request(QUrl(QString::fromStdString(GetDiscordAPI()) + QStringLiteral("users/@me/remote-auth/login")));
    QtHttpClient::applyClientHeaders(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/json"));
    const QByteArray body = QJsonDocument(QJsonObject{{QStringLiteral("ticket"), ticket}}).toJson(QJsonDocument::Compact);

    sendWithCaptcha(m_nam, request, "POST", body, m_captcha, this, [this](const HttpResult& result) {
        if (result.status != 200) {
            fail(PasswordLogin::describe(result));
            return;
        }
        const QJsonObject response = QJsonDocument::fromJson(result.body).object();
        const QByteArray token = decrypt(response.value(QStringLiteral("encrypted_token")).toString().toLatin1());
        if (token.isEmpty()) {
            fail(tr("Discord QR login failed. Tap refresh to try again."));
            return;
        }
        stop();
        setStatus(tr("Signed in."), false);
        emit tokenReceived(QString::fromUtf8(token));
    });
}

QByteArray RemoteAuth::decrypt(const QByteArray& base64) const
{
    if (!m_key || base64.isEmpty())
        return QByteArray();
    const QByteArray input = QByteArray::fromBase64(base64);

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(m_key, nullptr);
    if (!ctx)
        return QByteArray();

    QByteArray output;
    size_t length = 0;
    const auto* in = reinterpret_cast<const unsigned char*>(input.constData());
    if (EVP_PKEY_decrypt_init(ctx) > 0
        && EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) > 0
        && EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) > 0
        && EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) > 0
        && EVP_PKEY_decrypt(ctx, nullptr, &length, in, size_t(input.size())) > 0) {
        output.resize(qsizetype(length));
        if (EVP_PKEY_decrypt(ctx, reinterpret_cast<unsigned char*>(output.data()), &length, in,
                             size_t(input.size())) > 0)
            output.resize(qsizetype(length));
        else
            output.clear();
    }
    EVP_PKEY_CTX_free(ctx);
    return output;
}

QString RemoteAuth::renderQr(const QString& url)
{
    using qrcodegen::QrCode;
    const QrCode qr = QrCode::encodeText(url.toUtf8().constData(), QrCode::Ecc::MEDIUM);

    const int border = 2;
    const int scale = 8;
    const int size = (qr.getSize() + border * 2) * scale;

    QImage image(size, size, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (int y = 0; y < qr.getSize(); ++y) {
        for (int x = 0; x < qr.getSize(); ++x) {
            if (qr.getModule(x, y))
                painter.drawRect((x + border) * scale, (y + border) * scale, scale, scale);
        }
    }
    painter.end();

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
}
