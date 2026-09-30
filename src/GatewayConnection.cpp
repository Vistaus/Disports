#include "GatewayConnection.h"

#include <nlohmann/json.hpp>

#include "discord/DiscordInstance.hpp"
#include "discord/network/DiscordAPI.hpp"
#include "discord/network/DiscordRequest.hpp"
#include "discord/network/HTTPClient.hpp"

#include "Log.h"
#include "Session.h"
#include "backend/QtWebsocketClient.h"

namespace {

constexpr int MaxReconnectDelay = 30;

enum Opcode {
    Reconnect = 7,
    InvalidSession = 9,
    HeartbeatAck = 11,
};

// The opcode of a small payload (heartbeat ACK, RECONNECT, ...), or -1.
// Dispatches are large and left to the core.
int smallPayloadOpcode(const std::string& payload)
{
    if (payload.size() > 1024)
        return -1;
    try {
        const nlohmann::json json = nlohmann::json::parse(payload);
        if (json.contains("op") && json["op"].is_number_integer())
            return json["op"].get<int>();
    } catch (const std::exception&) {
    }
    return -1;
}

}

GatewayConnection::GatewayConnection(Session* session, QtWebsocketClient* sockets)
    : QObject(session)
    , m_session(session)
    , m_sockets(sockets)
{
    connect(&m_heartbeat, &QTimer::timeout, this, &GatewayConnection::heartbeat);
    m_reconnect.setSingleShot(true);
    connect(&m_reconnect, &QTimer::timeout, this, &GatewayConnection::start);
    m_countdown.setInterval(1000);
    connect(&m_countdown, &QTimer::timeout, this, [this]() {
        if (m_reconnectSeconds > 0) {
            --m_reconnectSeconds;
            emit reconnectSecondsChanged();
        }
        if (m_reconnectSeconds == 0)
            m_countdown.stop();
    });
}

void GatewayConnection::setConnected(bool connected)
{
    if (connected)
        m_reconnectDelay = 1;
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectedChanged();
}

void GatewayConnection::setNetworkOnline(bool online)
{
    if (m_networkOnline == online)
        return;
    m_networkOnline = online;
    emit networkOnlineChanged();

    if (!online) {
        // The socket can't survive this; don't wait for a heartbeat to fail.
        m_reconnect.stop();
        stopCountdown();
        drop();
    } else if (!m_connected) {
        open();
    }
}

void GatewayConnection::open()
{
    m_reconnectDelay = 1;
    start();
}

void GatewayConnection::reconnect()
{
    drop();
    open();
}

void GatewayConnection::close()
{
    m_heartbeat.stop();
    m_reconnect.stop();
    stopCountdown();
    m_sockets->abortAll();
    setConnected(false);
}

void GatewayConnection::start()
{
    m_reconnect.stop();
    stopCountdown();
    DiscordInstance* instance = m_session->instance();
    if (!instance || !m_networkOnline)
        return;

    if (instance->HasGatewayURL())
        instance->StartGatewaySession();
    else
        GetHTTPClient()->PerformRequest(false, NetRequest::GET, GetDiscordAPI() + "gateway", DiscordRequest::GATEWAY, 0);
}

void GatewayConnection::drop()
{
    m_heartbeat.stop();
    m_sockets->abortAll();
    if (DiscordInstance* instance = m_session->instance())
        instance->CloseGatewaySession();
    setConnected(false);
}

void GatewayConnection::scheduleReconnect()
{
    if (!m_session->instance() || !m_networkOnline || m_reconnect.isActive())
        return;
    m_reconnectSeconds = m_reconnectDelay;
    emit reconnectSecondsChanged();
    m_countdown.start();
    m_reconnect.start(m_reconnectDelay * 1000);
    m_reconnectDelay = qMin(m_reconnectDelay * 2, MaxReconnectDelay);
}

void GatewayConnection::stopCountdown()
{
    m_countdown.stop();
    if (m_reconnectSeconds != 0) {
        m_reconnectSeconds = 0;
        emit reconnectSecondsChanged();
    }
}

void GatewayConnection::heartbeat()
{
    DiscordInstance* instance = m_session->instance();
    if (!instance)
        return;
    if (!m_heartbeatAcked) {
        qCWarning(lcGateway, "heartbeat not acknowledged, reconnecting");
        drop();
        scheduleReconnect();
        return;
    }
    m_heartbeatAcked = false;
    instance->SendHeartbeat();
}

void GatewayConnection::heartbeatIntervalReceived(int ms)
{
    m_heartbeatAcked = true;
    m_heartbeat.start(qMax(ms, 1000));
}

void GatewayConnection::messageReceived(int gatewayId, const std::string& payload)
{
    DiscordInstance* instance = m_session->instance();
    if (!instance || gatewayId != instance->GetGatewayID())
        return;

    switch (smallPayloadOpcode(payload)) {
    case HeartbeatAck:
        m_heartbeatAcked = true;
        return;
    case Reconnect:
    case InvalidSession:
        // The core doesn't handle these: start a fresh session.
        drop();
        m_reconnectDelay = 1;
        scheduleReconnect();
        return;
    default:
        break;
    }

    try {
        instance->HandleGatewayMessage(payload);
    } catch (const std::exception& e) {
        qCWarning(lcGateway, "error handling a message: %s", e.what());
    }
}

void GatewayConnection::failed(int gatewayId, const QString& reason)
{
    DiscordInstance* instance = m_session->instance();
    if (!instance || (gatewayId >= 0 && gatewayId != instance->GetGatewayID()))
        return;
    qCWarning(lcGateway, "connection failed: %s", qPrintable(reason));
    m_heartbeat.stop();
    instance->m_gatewayConnId = -1;
    setConnected(false);
    scheduleReconnect();
}

void GatewayConnection::closed(int gatewayId, int code)
{
    DiscordInstance* instance = m_session->instance();
    if (!instance || gatewayId != instance->GetGatewayID())
        return;
    m_heartbeat.stop();
    instance->GatewayClosed(code);
}

void GatewayConnection::sessionClosed(int code)
{
    qCWarning(lcGateway, "session closed with code %d", code);
    m_heartbeat.stop();
    setConnected(false);
    scheduleReconnect();
}

void GatewayConnection::closedForReconnect()
{
    m_heartbeat.stop();
    setConnected(false);
    scheduleReconnect();
}
