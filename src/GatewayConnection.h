#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <string>

class QtWebsocketClient;
class Session;

// Keeps the connection to Discord's gateway alive: heartbeats (an
// unanswered one means the connection is dead even if the socket looks
// open), reconnecting with a growing delay, and following the network.
class GatewayConnection : public QObject
{
    Q_OBJECT
    // READY received on the current connection.
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool networkOnline READ networkOnline NOTIFY networkOnlineChanged)
    // Countdown to the next attempt, 0 when none is waiting.
    Q_PROPERTY(int reconnectSeconds READ reconnectSeconds NOTIFY reconnectSecondsChanged)

public:
    GatewayConnection(Session* session, QtWebsocketClient* sockets);

    bool connected() const { return m_connected; }
    bool networkOnline() const { return m_networkOnline; }
    void setNetworkOnline(bool online);
    int reconnectSeconds() const { return m_reconnectSeconds; }

    // Connects now, without waiting for the countdown.
    Q_INVOKABLE void reconnect();
    // After signing in.
    void open();
    // Before signing out.
    void close();
    void setConnected(bool connected);

    // From the core.
    void heartbeatIntervalReceived(int ms);
    void messageReceived(int gatewayId, const std::string& payload);
    void failed(int gatewayId, const QString& reason);
    void closed(int gatewayId, int code);
    void sessionClosed(int code);
    // Closed by Discord in a way that just needs a new connection.
    void closedForReconnect();

signals:
    void connectedChanged();
    void networkOnlineChanged();
    void reconnectSecondsChanged();

private:
    void start();
    void drop();
    void scheduleReconnect();
    void stopCountdown();
    void heartbeat();

    Session* m_session;
    QtWebsocketClient* m_sockets;
    bool m_connected = false;
    bool m_networkOnline = true;
    QTimer m_heartbeat;
    bool m_heartbeatAcked = true;
    QTimer m_reconnect;
    QTimer m_countdown;
    int m_reconnectDelay = 1; // seconds, doubling up to MaxReconnectDelay
    int m_reconnectSeconds = 0;
};
