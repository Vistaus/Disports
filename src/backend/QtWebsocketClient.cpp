#include "QtWebsocketClient.h"

#include <QNetworkRequest>
#include <QUrl>
#include <QWebSocket>

#include "discord/Frontend.hpp"
#include "discord/config/DiscordClientConfig.hpp"

QtWebsocketClient::QtWebsocketClient(QObject* parent)
    : QObject(parent)
{
}

QtWebsocketClient::~QtWebsocketClient()
{
    abortAll();
}

int QtWebsocketClient::Connect(const std::string& uri)
{
    const int id = m_nextId++;

    auto* socket = new QWebSocket(QStringLiteral("https://discord.com"),
                                  QWebSocketProtocol::VersionLatest, this);
    m_connections[id] = Connection{socket, false};

    connect(socket, &QWebSocket::connected, this, [this, id]() {
        auto it = m_connections.find(id);
        if (it != m_connections.end())
            it->second.opened = true;
    });
    connect(socket, &QWebSocket::textMessageReceived, this, [id](const QString& message) {
        GetFrontend()->OnWebsocketMessage(id, message.toStdString());
    });
    connect(socket, &QWebSocket::binaryMessageReceived, this, [id](const QByteArray& message) {
        GetFrontend()->OnWebsocketMessage(id, message.toStdString());
    });
    connect(socket, &QWebSocket::disconnected, this, [this, id, socket]() {
        finish(id, int(socket->closeCode()), socket->closeReason(), false);
    });
    connect(socket, &QWebSocket::errorOccurred, this, [this, id, socket](QAbstractSocket::SocketError) {
        auto it = m_connections.find(id);
        if (it == m_connections.end() || it->second.opened)
            return; // an open connection reports through disconnected()
        finish(id, int(socket->error()), socket->errorString(), true);
    });

    QNetworkRequest request(QUrl(QString::fromStdString(uri)));
    request.setRawHeader("User-Agent", QByteArray::fromStdString(GetClientConfig()->GetUserAgent()));
    socket->open(request);
    return id;
}

void QtWebsocketClient::Close(int id, int code)
{
    auto it = m_connections.find(id);
    if (it == m_connections.end())
        return;

    // A close we asked for is not reported back to the core: it has already
    // moved on (StartGatewaySession closes the old connection right before
    // opening a new one).
    QWebSocket* socket = it->second.socket;
    m_connections.erase(it);
    socket->disconnect(this);
    socket->close(QWebSocketProtocol::CloseCode(code));
    socket->deleteLater();
}

void QtWebsocketClient::SendMsg(int id, const std::string& msg)
{
    auto it = m_connections.find(id);
    if (it == m_connections.end() || !it->second.opened)
        return;
    it->second.socket->sendTextMessage(QString::fromStdString(msg));
}

void QtWebsocketClient::abortAll()
{
    for (auto& [id, connection] : m_connections) {
        connection.socket->disconnect(this);
        connection.socket->abort();
        connection.socket->deleteLater();
    }
    m_connections.clear();
}

void QtWebsocketClient::finish(int id, int code, const QString& reason, bool failedToOpen)
{
    auto it = m_connections.find(id);
    if (it == m_connections.end())
        return;

    QWebSocket* socket = it->second.socket;
    m_connections.erase(it);
    socket->disconnect(this);
    socket->deleteLater();

    const std::string message = reason.toStdString();
    if (failedToOpen)
        GetFrontend()->OnWebsocketFail(id, code, message, false, true);
    else
        GetFrontend()->OnWebsocketClose(id, code, message);
}
