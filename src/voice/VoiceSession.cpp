#include "VoiceSession.h"

#include <QDateTime>
#include <QNetworkDatagram>
#include <QUdpSocket>
#include <QWebSocket>

#include <nlohmann/json.hpp>
#include <opus/opus.h>

#include <cstdlib>
#include <cstring>

#include "DaveSession.h"
#include "Log.h"

using Json = nlohmann::json;

namespace {

enum VoiceOp {
    Identify = 0, SelectProtocol = 1, Ready = 2, Heartbeat = 3, SessionDescription = 4,
    Speaking = 5, HeartbeatAck = 6, Hello = 8, ClientsConnect = 11, ClientDisconnect = 13,
    DavePrepareTransition = 21, DaveExecuteTransition = 22, DaveTransitionReady = 23,
    DavePrepareEpoch = 24, MlsExternalSender = 25, MlsKeyPackage = 26, MlsProposals = 27,
    MlsCommitWelcome = 28, MlsAnnounceCommitTransition = 29, MlsWelcome = 30,
    MlsInvalidCommitWelcome = 31,
};

constexpr uint8_t OpusPayloadType = 120;
constexpr size_t RtpHeaderSize = 12;
constexpr int SilenceFramesAfterMute = 5;
constexpr qint64 SpeakingHoldMs = 400;
constexpr int64_t SelfSpeakingLevel = 400; // mean absolute sample
const uint8_t OpusSilence[] = {0xF8, 0xFF, 0xFE};

uint16_t be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }
uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
void putBe16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v >> 8); p[1] = uint8_t(v); }
void putBe32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v); }

std::string idOf(const Json& value)
{
    if (value.is_string())
        return value.get<std::string>();
    if (value.is_number_unsigned() || value.is_number_integer())
        return std::to_string(value.get<uint64_t>());
    return std::string();
}

}

VoiceSession::VoiceSession(const VoiceServerInfo& info)
    : m_info(info)
{
}

VoiceSession::~VoiceSession()
{
    stop();
    if (m_encoder)
        opus_encoder_destroy(m_encoder);
    for (auto& [ssrc, decoder] : m_decoders)
        opus_decoder_destroy(decoder);
}

void VoiceSession::start()
{
    emit stateChanged(QStringLiteral("connecting"));

    m_heartbeat = new QTimer(this);
    connect(m_heartbeat, &QTimer::timeout, this, &VoiceSession::sendHeartbeat);
    m_discoveryRetry = new QTimer(this);
    m_discoveryRetry->setInterval(1000);
    connect(m_discoveryRetry, &QTimer::timeout, this, &VoiceSession::sendIpDiscovery);
    m_speakingTimer = new QTimer(this);
    m_speakingTimer->setInterval(150);
    connect(m_speakingTimer, &QTimer::timeout, this, &VoiceSession::updateSpeaking);

    DaveSession::Callbacks callbacks;
    callbacks.sendKeyPackage = [this](const std::vector<uint8_t>& p) { sendBinary(MlsKeyPackage, p); };
    callbacks.sendCommitWelcome = [this](const std::vector<uint8_t>& p) { sendBinary(MlsCommitWelcome, p); };
    callbacks.sendReadyForTransition = [this](uint16_t id) {
        sendJson(DaveTransitionReady, Json{{"transition_id", id}}.dump());
    };
    callbacks.sendInvalidCommitWelcome = [this](uint16_t id) {
        sendJson(MlsInvalidCommitWelcome, Json{{"transition_id", id}}.dump());
    };
    // The MLS group is the voice channel (not the server).
    m_dave = std::make_unique<DaveSession>(m_info.userId.toStdString(), m_info.channelId.toULongLong(), callbacks);

    m_ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(m_ws, &QWebSocket::connected, this, &VoiceSession::onConnected);
    connect(m_ws, &QWebSocket::textMessageReceived, this, &VoiceSession::onText);
    connect(m_ws, &QWebSocket::binaryMessageReceived, this, &VoiceSession::onBinary);
    connect(m_ws, &QWebSocket::disconnected, this, &VoiceSession::onClosed);

    QString url = m_info.endpoint;
    if (!url.startsWith(QLatin1String("ws://")) && !url.startsWith(QLatin1String("wss://")))
        url = QStringLiteral("wss://") + url;
    m_ws->open(QUrl(url + QStringLiteral("/?v=8")));
}

void VoiceSession::stop()
{
    if (m_stopped)
        return;
    m_stopped = true;
    m_audio.stop();
    if (m_heartbeat)
        m_heartbeat->stop();
    if (m_speakingTimer)
        m_speakingTimer->stop();
    if (m_ws) {
        m_ws->disconnect(this);
        m_ws->close();
    }
    if (m_udp)
        m_udp->close();
    emit stateChanged(QStringLiteral("disconnected"));
}

void VoiceSession::fail(const QString& reason)
{
    qCWarning(lcVoice, "%s", qPrintable(reason));
    emit failed(reason);
    stop();
}

// Voice gateway

void VoiceSession::sendJson(int op, const std::string& data)
{
    if (!m_ws)
        return;
    m_ws->sendTextMessage(QString::fromStdString("{\"op\":" + std::to_string(op) + ",\"d\":" + data + "}"));
}

void VoiceSession::sendBinary(uint8_t op, const std::vector<uint8_t>& payload)
{
    if (!m_ws)
        return;
    QByteArray message;
    message.reserve(qsizetype(payload.size() + 1));
    message.append(char(op));
    message.append(reinterpret_cast<const char*>(payload.data()), qsizetype(payload.size()));
    m_ws->sendBinaryMessage(message);
}

void VoiceSession::onConnected()
{
    Json d = {
        {"server_id", m_info.serverId.toStdString()},
        {"channel_id", m_info.channelId.toStdString()},
        {"user_id", m_info.userId.toStdString()},
        {"session_id", m_info.sessionId.toStdString()},
        {"token", m_info.token.toStdString()},
        {"video", false},
        {"max_dave_protocol_version", DaveSession::maxProtocolVersion()},
    };
    sendJson(Identify, d.dump());
}

void VoiceSession::sendHeartbeat()
{
    Json d = {{"t", QDateTime::currentMSecsSinceEpoch()}};
    if (m_lastSeq >= 0)
        d["seq_ack"] = m_lastSeq;
    sendJson(Heartbeat, d.dump());
}

void VoiceSession::onText(const QString& text)
{
    Json message = Json::parse(text.toStdString(), nullptr, false);
    if (message.is_discarded() || !message.is_object())
        return;
    if (message.contains("seq") && message["seq"].is_number())
        m_lastSeq = message["seq"].get<int>();
    const int op = message.value("op", -1);
    const Json& d = message.contains("d") ? message["d"] : Json();

    switch (op) {
    case Hello:
        m_heartbeat->start(int(d.value("heartbeat_interval", 13750.0)));
        sendHeartbeat();
        break;
    case Ready: {
        m_ssrc = d.value("ssrc", 0u);
        m_serverAddress = QHostAddress(QString::fromStdString(d.value("ip", std::string())));
        m_serverPort = quint16(d.value("port", 0));
        m_modes.clear();
        if (d.contains("modes") && d["modes"].is_array())
            for (const Json& mode : d["modes"])
                if (mode.is_string())
                    m_modes.push_back(mode.get<std::string>());
        m_udp = new QUdpSocket(this);
        connect(m_udp, &QUdpSocket::readyRead, this, &VoiceSession::onDatagrams);
        if (!m_udp->bind(QHostAddress(QHostAddress::AnyIPv4), 0)) {
            fail(QStringLiteral("UDP: %1").arg(m_udp->errorString()));
            return;
        }
        sendIpDiscovery();
        m_discoveryRetry->start();
        break;
    }
    case SessionDescription: {
        std::vector<uint8_t> key;
        if (d.contains("secret_key") && d["secret_key"].is_array())
            for (const Json& b : d["secret_key"])
                key.push_back(uint8_t(b.get<int>()));
        startMedia(key, d.value("mode", std::string()), d.value("dave_protocol_version", 0));
        break;
    }
    case Speaking: {
        const std::string user = idOf(d.value("user_id", Json()));
        const uint32_t ssrc = d.value("ssrc", 0u);
        if (!user.empty() && ssrc) {
            m_ssrcUsers[ssrc] = user;
            addUser(user);
        }
        break;
    }
    case ClientsConnect:
        if (d.contains("user_ids") && d["user_ids"].is_array())
            for (const Json& id : d["user_ids"])
                addUser(idOf(id));
        break;
    case ClientDisconnect:
        removeUser(idOf(d.value("user_id", Json())));
        break;
    case DavePrepareTransition:
        m_dave->onPrepareTransition(uint16_t(d.value("transition_id", 0)), d.value("protocol_version", 0));
        break;
    case DaveExecuteTransition:
        m_dave->onExecuteTransition(uint16_t(d.value("transition_id", 0)));
        break;
    case DavePrepareEpoch:
        m_dave->onPrepareEpoch(d.value("epoch", uint64_t(0)), d.value("protocol_version", 0));
        break;
    default:
        break;
    }
}

void VoiceSession::onBinary(const QByteArray& data)
{
    // Server binary messages: sequence (2 bytes), opcode, payload.
    if (data.size() < 3)
        return;
    const auto* bytes = reinterpret_cast<const uint8_t*>(data.constData());
    m_lastSeq = be16(bytes);
    const uint8_t op = bytes[2];
    const std::vector<uint8_t> payload(bytes + 3, bytes + data.size());

    switch (op) {
    case MlsExternalSender:
        m_dave->onExternalSenderPackage(payload);
        break;
    case MlsProposals:
        m_dave->onProposals(payload);
        break;
    case MlsAnnounceCommitTransition:
    case MlsWelcome: {
        if (payload.size() < 2)
            return;
        const uint16_t transition = be16(payload.data());
        const std::vector<uint8_t> body(payload.begin() + 2, payload.end());
        if (op == MlsWelcome)
            m_dave->onWelcome(transition, body);
        else
            m_dave->onAnnounceCommitTransition(transition, body);
        break;
    }
    default:
        break;
    }
}

void VoiceSession::onClosed()
{
    if (m_stopped)
        return;
    const int code = int(m_ws->closeCode());
    // 4017: the call needs end-to-end encryption we do not support
    fail(code == 4017 ? QStringLiteral("This call requires end-to-end encryption (DAVE)")
                      : QStringLiteral("The voice connection closed (%1 %2)").arg(code).arg(m_ws->closeReason()));
}

// UDP

void VoiceSession::sendIpDiscovery()
{
    if (++m_discoveryTries > 5) {
        m_discoveryRetry->stop();
        fail(QStringLiteral("The voice server did not answer over UDP"));
        return;
    }
    uint8_t packet[74] = {};
    putBe16(packet, 0x1);
    putBe16(packet + 2, 70);
    putBe32(packet + 4, m_ssrc);
    m_udp->writeDatagram(reinterpret_cast<const char*>(packet), sizeof(packet), m_serverAddress, m_serverPort);
}

void VoiceSession::onDatagrams()
{
    while (m_udp && m_udp->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_udp->receiveDatagram();
        const QByteArray data = datagram.data();
        const auto* bytes = reinterpret_cast<const uint8_t*>(data.constData());
        const size_t size = size_t(data.size());

        // IP discovery response
        if (size == 74 && be16(bytes) == 0x2) {
            if (!m_discoveryRetry->isActive())
                continue;
            m_discoveryRetry->stop();
            const std::string address(reinterpret_cast<const char*>(bytes + 8), strnlen(reinterpret_cast<const char*>(bytes + 8), 64));
            const uint16_t port = be16(bytes + 72);
            const TransportCipher::Mode mode = TransportCipher::pick(m_modes);
            if (mode == TransportCipher::None) {
                fail(QStringLiteral("No supported voice encryption mode"));
                return;
            }
            Json d = {
                {"protocol", "udp"},
                {"data", {{"address", address}, {"port", port}, {"mode", TransportCipher::name(mode)}}},
                {"codecs", Json::array({Json{{"name", "opus"}, {"type", "audio"}, {"priority", 1000}, {"payload_type", OpusPayloadType}}})},
            };
            sendJson(SelectProtocol, d.dump());
            continue;
        }
        receiveRtp(bytes, size);
    }
}

void VoiceSession::startMedia(const std::vector<uint8_t>& key, const std::string& mode, int daveVersion)
{
    m_cipher.setKey(TransportCipher::fromName(mode), key);
    if (!m_cipher.ready()) {
        fail(QStringLiteral("Unusable voice session key or mode (%1)").arg(QString::fromStdString(mode)));
        return;
    }
    m_dave->onSelectProtocolAck(daveVersion);

    int error = 0;
    // Discord's clients send stereo; the mono microphone goes in both.
    m_encoder = opus_encoder_create(AudioIO::SampleRate, 2, OPUS_APPLICATION_VOIP, &error);
    if (!m_encoder) {
        fail(QStringLiteral("Opus encoder: %1").arg(opus_strerror(error)));
        return;
    }
    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(64000));
    opus_encoder_ctl(m_encoder, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(m_encoder, OPUS_SET_PACKET_LOSS_PERC(5));

    // Frames arrive on PulseAudio's thread; encode and send here.
    const bool audioOk = m_audio.start([this](const int16_t* samples) {
        const QByteArray pcm(reinterpret_cast<const char*>(samples), AudioIO::FrameSamples * qsizetype(sizeof(int16_t)));
        QMetaObject::invokeMethod(this, [this, pcm]() { sendFrame(pcm); }, Qt::QueuedConnection);
    }, m_info.voiceProcessing, m_info.noiseSuppression);
    if (!audioOk) {
        fail(QStringLiteral("Audio: %1").arg(QString::fromStdString(m_audio.error())));
        return;
    }
    emit speakerAvailable(m_audio.hasEarpiece());
    m_audio.setCaptureEnabled(!m_muted);
    m_audio.setPlaybackEnabled(!m_deafened);
    m_speakingTimer->start();
    sendSpeaking(!m_muted);
    emit stateChanged(QStringLiteral("connected"));
}

void VoiceSession::sendSpeaking(bool speaking)
{
    if (m_speaking == speaking)
        return;
    m_speaking = speaking;
    sendJson(Speaking, Json{{"speaking", speaking ? 1 : 0}, {"delay", 0}, {"ssrc", m_ssrc}}.dump());
}

void VoiceSession::sendFrame(const QByteArray& pcm)
{
    if (m_stopped || !m_encoder || !m_cipher.ready())
        return;
    if (m_muted)
        return;
    // Our own speaking indicator.
    const auto* samples = reinterpret_cast<const int16_t*>(pcm.constData());
    int64_t level = 0;
    for (int i = 0; i < AudioIO::FrameSamples; ++i)
        level += std::abs(int(samples[i]));
    if (level / AudioIO::FrameSamples > SelfSpeakingLevel)
        m_lastHeard[m_info.userId.toStdString()] = QDateTime::currentMSecsSinceEpoch();
    int16_t stereo[AudioIO::FrameSamples * 2];
    for (int i = 0; i < AudioIO::FrameSamples; ++i)
        stereo[2 * i] = stereo[2 * i + 1] = samples[i];
    std::vector<uint8_t> opus(1276);
    const int size = opus_encode(m_encoder, stereo, AudioIO::FrameSamples, opus.data(), opus_int32(opus.size()));
    if (size <= 0)
        return;
    opus.resize(size_t(size));
    sendOpus(opus);
}

void VoiceSession::sendOpus(const std::vector<uint8_t>& opus)
{
    std::vector<uint8_t> frame;
    if (!m_dave->encrypt(m_ssrc, opus, frame))
        return; // no end-to-end key yet

    uint8_t header[RtpHeaderSize];
    header[0] = 0x80;
    header[1] = OpusPayloadType;
    putBe16(header + 2, m_sequence++);
    putBe32(header + 4, m_timestamp);
    putBe32(header + 8, m_ssrc);
    m_timestamp += AudioIO::FrameSamples;

    std::vector<uint8_t> sealed;
    const uint32_t nonce = m_nonce++;
    if (!m_cipher.seal(header, sizeof(header), frame.data(), frame.size(), nonce, sealed))
        return;
    QByteArray packet;
    packet.reserve(qsizetype(sizeof(header) + sealed.size() + 4));
    packet.append(reinterpret_cast<const char*>(header), sizeof(header));
    packet.append(reinterpret_cast<const char*>(sealed.data()), qsizetype(sealed.size()));
    uint8_t suffix[4];
    putBe32(suffix, nonce);
    packet.append(reinterpret_cast<const char*>(suffix), 4);
    m_udp->writeDatagram(packet, m_serverAddress, m_serverPort);
}

void VoiceSession::receiveRtp(const uint8_t* packet, size_t size)
{
    if (!m_cipher.ready() || size < RtpHeaderSize + TransportCipher::TagSize + 4)
        return;
    if ((packet[0] & 0xC0) != 0x80)
        return;
    // RTCP (sender/receiver reports): not used.
    if (packet[1] >= 200 && packet[1] <= 206)
        return;
    if ((packet[1] & 0x7F) != OpusPayloadType)
        return;

    const uint32_t ssrc = be32(packet + 8);
    const size_t csrcs = packet[0] & 0x0F;
    const bool extension = packet[0] & 0x10;
    size_t headerSize = RtpHeaderSize + 4 * csrcs;
    size_t extensionBytes = 0;
    if (extension) {
        // RTP-size modes: the extension preamble is clear, its data encrypted.
        if (size < headerSize + 4)
            return;
        extensionBytes = size_t(be16(packet + headerSize + 2)) * 4;
        headerSize += 4;
    }
    if (size < headerSize + TransportCipher::TagSize + 4)
        return;
    const uint32_t nonce = be32(packet + size - 4);
    std::vector<uint8_t> plain;
    if (!m_cipher.open(packet, headerSize, packet + headerSize, size - headerSize - 4, nonce, plain))
        return;
    if (plain.size() < extensionBytes)
        return;
    std::vector<uint8_t> frame(plain.begin() + long(extensionBytes), plain.end());

    auto userIt = m_ssrcUsers.find(ssrc);
    if (userIt == m_ssrcUsers.end())
        return; // not announced yet (Speaking)
    const std::string& user = userIt->second;

    std::vector<uint8_t> opus;
    if (!m_dave->decrypt(user, frame, opus))
        return;
    if (opus.size() == sizeof(OpusSilence) && std::memcmp(opus.data(), OpusSilence, sizeof(OpusSilence)) == 0)
        return;

    OpusDecoder*& decoder = m_decoders[ssrc];
    if (!decoder) {
        int error = 0;
        decoder = opus_decoder_create(AudioIO::SampleRate, 2, &error);
        if (!decoder)
            return;
    }
    int16_t pcm[AudioIO::FrameSamples * 2 * 6]; // up to 120 ms
    const int samples = opus_decode(decoder, opus.data(), opus_int32(opus.size()), pcm, AudioIO::FrameSamples * 6, 0);
    for (int offset = 0; offset + AudioIO::FrameSamples <= samples; offset += AudioIO::FrameSamples)
        m_audio.play(ssrc, pcm + offset * 2);
    m_lastHeard[user] = QDateTime::currentMSecsSinceEpoch();
}

// Participants

void VoiceSession::addUser(const std::string& userId)
{
    if (userId.empty() || userId == m_info.userId.toStdString() || !m_users.insert(userId).second)
        return;
    m_dave->addUser(userId);
    QStringList ids;
    for (const std::string& id : m_users)
        ids.append(QString::fromStdString(id));
    emit usersChanged(ids);
}

void VoiceSession::removeUser(const std::string& userId)
{
    if (!m_users.erase(userId))
        return;
    m_dave->removeUser(userId);
    for (auto it = m_ssrcUsers.begin(); it != m_ssrcUsers.end();) {
        if (it->second == userId) {
            m_audio.removeSource(it->first);
            auto decoder = m_decoders.find(it->first);
            if (decoder != m_decoders.end()) {
                opus_decoder_destroy(decoder->second);
                m_decoders.erase(decoder);
            }
            it = m_ssrcUsers.erase(it);
        } else {
            ++it;
        }
    }
    if (m_speakingUsers.erase(userId))
        emit speakingChanged(QString::fromStdString(userId), false);
    QStringList ids;
    for (const std::string& id : m_users)
        ids.append(QString::fromStdString(id));
    emit usersChanged(ids);
}

void VoiceSession::updateSpeaking()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const auto& [user, heard] : m_lastHeard) {
        const bool speaking = now - heard < SpeakingHoldMs;
        const bool was = m_speakingUsers.count(user) != 0;
        if (speaking == was)
            continue;
        if (speaking)
            m_speakingUsers.insert(user);
        else
            m_speakingUsers.erase(user);
        emit speakingChanged(QString::fromStdString(user), speaking);
    }
}

// Controls

void VoiceSession::setMuted(bool muted)
{
    if (m_muted == muted)
        return;
    m_muted = muted;
    m_audio.setCaptureEnabled(!muted);
    if (muted && m_cipher.ready()) {
        // Tell receivers the stream stops, as Discord clients do.
        for (int i = 0; i < SilenceFramesAfterMute; ++i)
            sendOpus(std::vector<uint8_t>(OpusSilence, OpusSilence + sizeof(OpusSilence)));
    }
    if (m_cipher.ready())
        sendSpeaking(!muted);
}

void VoiceSession::setDeafened(bool deafened)
{
    m_deafened = deafened;
    m_audio.setPlaybackEnabled(!deafened);
}

void VoiceSession::setSpeaker(bool speaker)
{
    m_audio.setSpeaker(speaker);
}
