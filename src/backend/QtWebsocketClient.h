#pragma once

#include <QObject>
#include <QString>

#include <map>

#include "discord/network/WebsocketClient.hpp"

class QWebSocket;

// WebsocketClient for the core, backed by QWebSocket.
// Events are reported through the Frontend (OnWebsocketMessage / Close /
// Fail) on the Qt event loop.
class QtWebsocketClient : public QObject, public WebsocketClient
{
    Q_OBJECT

public:
    explicit QtWebsocketClient(QObject* parent = nullptr);
    ~QtWebsocketClient() override;

    int Connect(const std::string& uri) override;
    void Close(int id, int code) override;
    void SendMsg(int id, const std::string& msg) override;

    // Drops every connection without notifying the core (network lost,
    // logout).
    void abortAll();

private:
    struct Connection {
        QWebSocket* socket = nullptr;
        bool opened = false;
    };

    // Reports the end of a connection once and forgets it.
    void finish(int id, int code, const QString& reason, bool failedToOpen);

    std::map<int, Connection> m_connections;
    int m_nextId = 1;
};
