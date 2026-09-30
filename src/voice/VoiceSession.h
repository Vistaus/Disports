#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "AudioIO.h"
#include "TransportCipher.h"

class QUdpSocket;
class QWebSocket;
class DaveSession;
struct OpusEncoder;
struct OpusDecoder;

// Where to connect, from the main gateway's VOICE_STATE_UPDATE (session id)
// and VOICE_SERVER_UPDATE (endpoint, token).
struct VoiceServerInfo {
    QString endpoint;  // host:port, or a ws:// / wss:// URL (tests)
    QString token;
    QString sessionId;
    QString serverId;  // the guild, or the DM channel for calls
    QString channelId;
    QString userId;
    bool voiceProcessing = true; // see AudioIO
    bool noiseSuppression = true;
};

// One voice connection, on its own thread: the voice gateway (v8, with
// DAVE), UDP media (RTP, transport encryption), Opus and the call audio.
//
//   voice gateway: Identify -> Hello/Ready -> IP discovery over UDP ->
//   Select Protocol -> Session Description (key, DAVE version) -> media
//   send: mic 20 ms -> Opus -> DAVE -> RTP -> transport encryption -> UDP
//   receive: the reverse, then one decoder per sender, mixed by AudioIO
class VoiceSession : public QObject
{
    Q_OBJECT

public:
    explicit VoiceSession(const VoiceServerInfo& info);
    ~VoiceSession() override;

public slots:
    void start();
    void stop();
    void setMuted(bool muted);
    void setDeafened(bool deafened);
    void setSpeaker(bool speaker);

signals:
    // "connecting", "connected", "disconnected"
    void stateChanged(const QString& state);
    void failed(const QString& reason);
    // The users in the voice connection (besides us), and who is talking.
    void usersChanged(const QStringList& userIds);
    void speakingChanged(const QString& userId, bool speaking);
    void speakerAvailable(bool available);

private:
    void onConnected();
    void onText(const QString& text);
    void onBinary(const QByteArray& data);
    void onClosed();
    void onDatagrams();
    void sendJson(int op, const std::string& data);
    void sendBinary(uint8_t op, const std::vector<uint8_t>& payload);
    void sendHeartbeat();
    void sendIpDiscovery();
    void startMedia(const std::vector<uint8_t>& key, const std::string& mode, int daveVersion);
    void sendSpeaking(bool speaking);
    void sendFrame(const QByteArray& pcm);
    void sendOpus(const std::vector<uint8_t>& opus);
    void receiveRtp(const uint8_t* packet, size_t size);
    void addUser(const std::string& userId);
    void removeUser(const std::string& userId);
    void updateSpeaking();
    void fail(const QString& reason);

    VoiceServerInfo m_info;
    QWebSocket* m_ws = nullptr;
    QUdpSocket* m_udp = nullptr;
    QTimer* m_heartbeat = nullptr;
    QTimer* m_discoveryRetry = nullptr;
    QTimer* m_speakingTimer = nullptr;
    int m_lastSeq = -1;
    bool m_stopped = false;

    // UDP
    QHostAddress m_serverAddress;
    quint16 m_serverPort = 0;
    uint32_t m_ssrc = 0;
    std::vector<std::string> m_modes;
    TransportCipher m_cipher;
    int m_discoveryTries = 0;

    // Sending
    AudioIO m_audio;
    OpusEncoder* m_encoder = nullptr;
    uint16_t m_sequence = 0;
    uint32_t m_timestamp = 0;
    uint32_t m_nonce = 0;
    bool m_muted = false;
    bool m_deafened = false;
    bool m_speaking = false;

    // Receiving
    std::unique_ptr<DaveSession> m_dave;
    std::map<uint32_t, std::string> m_ssrcUsers;
    std::map<uint32_t, OpusDecoder*> m_decoders;
    std::set<std::string> m_users;
    std::map<std::string, qint64> m_lastHeard;   // user -> ms
    std::set<std::string> m_speakingUsers;
};
