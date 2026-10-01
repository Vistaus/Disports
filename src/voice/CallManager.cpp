#include "CallManager.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QThread>

#include "discord/DiscordInstance.hpp"
#include "discord/models/Permissions.hpp"

#include "DiscordUrls.h"
#include "Ringtone.h"
#include "Session.h"
#include "VoiceSession.h"
#include "models/ChannelListModel.h"

namespace {

Snowflake snowflakeOf(const nlohmann::json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key))
        return 0;
    const nlohmann::json& value = object[key];
    if (value.is_string())
        return Snowflake(std::stoull(value.get<std::string>()));
    if (value.is_number_unsigned())
        return Snowflake(value.get<uint64_t>());
    return 0;
}

QString stringOf(const nlohmann::json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_string())
        return QString();
    return QString::fromStdString(object[key].get<std::string>());
}

QDBusInterface* unityScreen()
{
    // Allowed to confined apps by the keep-display-on policy group.
    static QDBusInterface* screen = new QDBusInterface(
        QStringLiteral("com.canonical.Unity.Screen"), QStringLiteral("/com/canonical/Unity/Screen"),
        QStringLiteral("com.canonical.Unity.Screen"), QDBusConnection::systemBus());
    return screen;
}

}

CallManager::CallManager(Session* session)
    : QObject(session)
    , m_session(session)
{
    m_clock.setInterval(1000);
    connect(&m_clock, &QTimer::timeout, this, &CallManager::statusTextChanged);
    // As on Discord: alone in a DM or group call for 3 minutes, we leave.
    m_aloneTimer.setSingleShot(true);
    m_aloneTimer.setInterval(3 * 60 * 1000);
    connect(&m_aloneTimer, &QTimer::timeout, this, [this]() {
        endCall();
        emit notice(tr("You left the call: no one else joined for 3 minutes."));
    });
    m_ringtone = new Ringtone(session->networkAccessManager(), this);
    connect(session->voiceStates(), &VoiceStates::userFlagsChanged, this, [this](Snowflake user) {
        if (m_participants.contains(user))
            emit participantsChanged();
    });
}

CallManager::~CallManager()
{
    stopVoice();
    keepDisplayOn(false);
}

QString CallManager::channelId() const
{
    return m_channel ? DiscordUrls::id(m_channel) : QString();
}

QString CallManager::initials() const
{
    return DiscordUrls::initials(m_title);
}

QString CallManager::statusText() const
{
    switch (m_state) {
    case Idle:
        return QString();
    case Incoming:
        return tr("Incoming call");
    case Connecting:
        return tr("Connecting...");
    case Active:
        break;
    }
    if (m_direct && m_participants.isEmpty())
        return m_ringing.isEmpty() ? tr("Waiting for others...") : tr("Calling...");
    if (!m_talkStartedMs)
        return tr("Connected");
    const qint64 seconds = (QDateTime::currentMSecsSinceEpoch() - m_talkStartedMs) / 1000;
    const QString mmss = QStringLiteral("%1:%2").arg(seconds / 60 % 60, 2, 10, QLatin1Char('0'))
                                                .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    return seconds >= 3600 ? QStringLiteral("%1:%2").arg(seconds / 3600).arg(mmss) : mmss;
}

QVariantList CallManager::participants() const
{
    QVariantList list;
    DiscordInstance* instance = m_session->instance();
    if (!instance || m_state == Idle)
        return list;
    auto entry = [this](Snowflake id, bool self, bool joined, bool muted, bool deafened, bool ringing = false) {
        Profile* profile = GetProfileCache()->LookupProfile(id, "", "", "", false);
        const QString name = !profile ? QString()
            : QString::fromStdString(profile->m_globalName.empty() ? profile->m_name : profile->m_globalName);
        return QVariantMap{
            {QStringLiteral("id"), DiscordUrls::id(id)},
            {QStringLiteral("name"), name},
            {QStringLiteral("avatarUrl"), DiscordUrls::userAvatar(id, profile ? profile->m_avatarlnk : std::string(), 256)},
            {QStringLiteral("speaking"), joined && m_speaking.contains(id)},
            {QStringLiteral("muted"), muted},
            {QStringLiteral("deafened"), deafened},
            {QStringLiteral("self"), self},
            {QStringLiteral("joined"), joined},
            {QStringLiteral("ringing"), ringing},
        };
    };
    const Snowflake me = instance->GetUserID();
    if (m_state != Incoming)
        list.append(entry(me, true, m_voiceConnected, m_muted, m_deafened));
    VoiceStates* states = m_session->voiceStates();
    for (Snowflake id : m_participants)
        list.append(entry(id, false, true, states->isMuted(id), states->isDeafened(id)));
    // DM calls: the others who have not picked up (yet).
    if (m_direct) {
        if (Channel* channel = instance->GetChannel(m_channel)) {
            for (Snowflake id : channel->m_recipients) {
                if (id != me && !m_participants.contains(id))
                    list.append(entry(id, false, false, false, false, m_ringing.contains(id)));
            }
        }
    }
    return list;
}

bool CallManager::canCall(const QString& channelId) const
{
    DiscordInstance* instance = m_session->instance();
    Channel* channel = instance ? instance->GetChannel(DiscordUrls::fromId(channelId)) : nullptr;
    if (!channel || !m_session->connected())
        return false;
    if (channel->IsDM())
        return true;
    return channel->IsVoice() && channel->HasPermission(PERM_CONNECT);
}

void CallManager::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    keepDisplayOn(state != Idle);
    updateRingtone();
    emit stateChanged();
    emit statusTextChanged();
}

void CallManager::describe(Snowflake channelId)
{
    DiscordInstance* instance = m_session->instance();
    Channel* channel = instance ? instance->GetChannel(channelId) : nullptr;
    m_channel = channelId;
    m_guild = channel && !channel->IsDM() ? channel->m_parentGuild : 0;
    m_direct = !channel || channel->IsDM();
    m_canSpeak = !channel || channel->IsDM() || channel->HasPermission(PERM_SPEAK);
    m_title = channel ? ChannelListModel::displayName(*channel) : QString();
    m_avatarUrl = channel ? ChannelListModel::iconUrl(*channel) : QString();
    if (channel && channel->m_channelType == Channel::DM && channel->m_recipients.size() == 1) {
        // A bigger picture of the other person for the call screen.
        const Snowflake other = channel->m_recipients.front();
        Profile* profile = GetProfileCache()->LookupProfile(other, "", "", "", false);
        m_avatarUrl = DiscordUrls::userAvatar(other, profile ? profile->m_avatarlnk : std::string(), 256);
    }
}

void CallManager::start(const QString& channelId)
{
    if (!canCall(channelId))
        return;
    const Snowflake channel = DiscordUrls::fromId(channelId);
    if (m_state != Idle && m_channel == channel) {
        emit showRequested();
        return;
    }
    // Straight from one call to the other, without closing the call screen.
    if (m_state != Idle)
        leave();
    describe(channel);
    // Joining a call already going on does not ring everyone again.
    const bool newCall = !m_session->voiceStates()->hasCall(channel);
    join(channel);
    if (m_direct && newCall) {
        m_session->instance()->RequestRingCall(channel);
        if (Channel* dm = m_session->instance()->GetChannel(channel)) {
            const Snowflake me = m_session->instance()->GetUserID();
            for (Snowflake id : dm->m_recipients) {
                if (id != me)
                    m_ringing.insert(id);
            }
        }
    }
    emit showRequested();
}

void CallManager::show()
{
    if (m_state != Idle)
        emit showRequested();
}

void CallManager::join(Snowflake channel)
{
    m_voiceSessionId.clear();
    m_voiceToken.clear();
    m_voiceEndpoint.clear();
    m_participants.clear();
    m_speaking.clear();
    m_ringing.clear();
    m_talkStartedMs = 0;
    m_joinPending = true;
    emit participantsChanged();
    if (!m_canSpeak && !m_muted) {
        m_muted = true;
        m_mutedByPermission = true;
        emit mutedChanged();
    }
    if (m_state == Connecting) {
        // Switched calls: a new title and picture.
        emit stateChanged();
        emit statusTextChanged();
    }
    setState(Connecting);
    updateAlone();
    m_session->instance()->SendVoiceStateUpdate(m_guild, channel, m_muted, m_deafened);
}

void CallManager::accept()
{
    if (m_state != Incoming)
        return;
    join(m_channel);
}

void CallManager::decline()
{
    if (m_state != Incoming)
        return;
    if (DiscordInstance* instance = m_session->instance())
        instance->RequestStopRinging(m_channel);
    endCall();
}

void CallManager::hangUp()
{
    if (m_state == Incoming) {
        decline();
        return;
    }
    endCall();
}

void CallManager::leave()
{
    const bool joined = m_state == Connecting || m_state == Active;
    stopVoice();
    if (joined) {
        if (DiscordInstance* instance = m_session->instance())
            instance->SendVoiceStateUpdate(0, 0, m_muted, m_deafened);
    }
    m_clock.stop();
    m_aloneTimer.stop();
    m_joinPending = false;
    if (m_mutedByPermission) {
        m_mutedByPermission = false;
        m_muted = false;
        emit mutedChanged();
    }
    m_participants.clear();
    m_speaking.clear();
    m_ringing.clear();
    m_channel = 0;
    m_guild = 0;
    emit participantsChanged();
}

void CallManager::endCall(const QString& reason)
{
    leave();
    setState(Idle);
    if (!reason.isEmpty())
        emit callFailed(reason);
}

void CallManager::gatewayLost()
{
    if (m_state != Idle)
        endCall(tr("The call ended: the connection to Discord was lost."));
}

void CallManager::toggleMute()
{
    if (m_muted && !m_canSpeak) {
        emit notice(tr("You don't have permission to speak in this channel."));
        return;
    }
    m_muted = !m_muted;
    if (!m_muted)
        m_deafened = false;
    emit mutedChanged();
    emit participantsChanged();
    if (m_voice) {
        QMetaObject::invokeMethod(m_voice, "setMuted", Q_ARG(bool, m_muted));
        QMetaObject::invokeMethod(m_voice, "setDeafened", Q_ARG(bool, m_deafened));
    }
    if (m_state == Connecting || m_state == Active)
        m_session->instance()->SendVoiceStateUpdate(m_guild, m_channel, m_muted, m_deafened);
}

void CallManager::toggleDeafen()
{
    m_deafened = !m_deafened;
    // Deafening mutes too, as on Discord; undeafening unmutes only if we
    // weren't muted before.
    if (m_deafened) {
        m_mutedBeforeDeafen = m_muted;
        m_muted = true;
    } else {
        m_muted = m_mutedBeforeDeafen || !m_canSpeak;
    }
    emit mutedChanged();
    emit participantsChanged();
    if (m_voice) {
        QMetaObject::invokeMethod(m_voice, "setMuted", Q_ARG(bool, m_muted));
        QMetaObject::invokeMethod(m_voice, "setDeafened", Q_ARG(bool, m_deafened));
    }
    if (m_state == Connecting || m_state == Active)
        m_session->instance()->SendVoiceStateUpdate(m_guild, m_channel, m_muted, m_deafened);
}

void CallManager::toggleSpeaker()
{
    if (!m_speakerAvailable)
        return;
    m_speaker = !m_speaker;
    emit speakerChanged();
    if (m_voice)
        QMetaObject::invokeMethod(m_voice, "setSpeaker", Q_ARG(bool, m_speaker));
}

// Gateway events

void CallManager::gatewayDispatch(const std::string& type, const nlohmann::json& message)
{
    if (type == "READY") {
        // Ready for the first call, without slowing down the start.
        QTimer::singleShot(5000, m_ringtone, &Ringtone::fetch);
        return;
    }
    if (type.rfind("VOICE_", 0) != 0 && type.rfind("CALL_", 0) != 0)
        return;
    DiscordInstance* instance = m_session->instance();
    if (!instance || !message.contains("d"))
        return;
    const nlohmann::json& d = message["d"];
    const Snowflake me = instance->GetUserID();

    if (type == "VOICE_STATE_UPDATE") {
        const Snowflake user = snowflakeOf(d, "user_id");
        const Snowflake channel = snowflakeOf(d, "channel_id");
        if (user == me) {
            if (m_state == Idle)
                return;
            if (channel != m_channel) {
                // Leaving the previous call, when switching.
                if (m_joinPending)
                    return;
                // Moved or disconnected elsewhere (another device).
                endCall();
                return;
            }
            m_joinPending = false;
            m_voiceSessionId = stringOf(d, "session_id");
            maybeConnect();
        } else if (m_channel && channel == m_channel && m_state != Incoming) {
            addParticipant(user);
        } else {
            removeParticipant(user);
        }
    } else if (type == "VOICE_SERVER_UPDATE") {
        if (m_state != Connecting && m_state != Active)
            return;
        const Snowflake server = snowflakeOf(d, "guild_id") ? snowflakeOf(d, "guild_id") : snowflakeOf(d, "channel_id");
        if (server && server != (m_guild ? m_guild : m_channel))
            return;
        m_voiceToken = stringOf(d, "token");
        // null while Discord picks a server; another update follows.
        m_voiceEndpoint = stringOf(d, "endpoint");
        // A new server (region change): reconnect.
        stopVoice();
        maybeConnect();
    } else if (type == "CALL_CREATE" || type == "CALL_UPDATE") {
        const Snowflake channel = snowflakeOf(d, "channel_id");
        bool ringingUs = false;
        if (d.contains("ringing") && d["ringing"].is_array()) {
            for (const nlohmann::json& id : d["ringing"]) {
                if ((id.is_string() && Snowflake(std::stoull(id.get<std::string>())) == me)
                        || (id.is_number_unsigned() && Snowflake(id.get<uint64_t>()) == me))
                    ringingUs = true;
            }
        }
        if (ringingUs && m_state == Idle) {
            describe(channel);
            m_participants.clear();
            setState(Incoming);
            emit showRequested();
        } else if (!ringingUs && m_state == Incoming && channel == m_channel) {
            // Answered elsewhere, or the caller gave up.
            endCall();
        }
        if (channel == m_channel && m_state != Idle && m_state != Incoming && d.contains("ringing")
                && d["ringing"].is_array()) {
            // Who is still being rung: not those who declined or didn't
            // answer.
            m_ringing.clear();
            for (const nlohmann::json& id : d["ringing"])
                m_ringing.insert(id.is_string() ? Snowflake(std::stoull(id.get<std::string>()))
                                                : Snowflake(id.is_number_unsigned() ? id.get<uint64_t>() : 0));
            emit participantsChanged();
            emit statusTextChanged();
        }
        if (channel == m_channel && m_state != Idle && m_state != Incoming && d.contains("voice_states")
                && d["voice_states"].is_array()) {
            for (const nlohmann::json& state : d["voice_states"]) {
                const Snowflake user = snowflakeOf(state, "user_id");
                if (user != me)
                    addParticipant(user);
            }
        }
    } else if (type == "CALL_DELETE") {
        if (snowflakeOf(d, "channel_id") == m_channel && m_state != Idle)
            endCall();
    }
}

void CallManager::addParticipant(Snowflake user)
{
    if (!user || user == m_session->instance()->GetUserID() || m_participants.contains(user))
        return;
    m_participants.append(user);
    m_ringing.remove(user);
    updateAlone();
    if (m_direct && m_voiceConnected && !m_talkStartedMs) {
        m_talkStartedMs = QDateTime::currentMSecsSinceEpoch();
        m_clock.start();
    }
    emit participantsChanged();
    emit statusTextChanged();
}

void CallManager::removeParticipant(Snowflake user)
{
    if (!m_participants.removeOne(user))
        return;
    m_speaking.remove(user);
    emit participantsChanged();
    emit statusTextChanged();
    updateAlone();
}

void CallManager::updateAlone()
{
    const bool alone = m_direct && (m_state == Connecting || m_state == Active) && m_participants.isEmpty();
    if (!alone)
        m_aloneTimer.stop();
    else if (!m_aloneTimer.isActive())
        m_aloneTimer.start();
}

// The voice connection

void CallManager::maybeConnect()
{
    if (m_voice || m_voiceSessionId.isEmpty() || m_voiceToken.isEmpty() || m_voiceEndpoint.isEmpty())
        return;

    VoiceServerInfo info;
    info.endpoint = m_voiceEndpoint;
    info.token = m_voiceToken;
    info.sessionId = m_voiceSessionId;
    info.serverId = DiscordUrls::id(m_guild ? m_guild : m_channel);
    info.channelId = DiscordUrls::id(m_channel);
    info.userId = DiscordUrls::id(m_session->instance()->GetUserID());
    info.voiceProcessing = m_session->preferences()->voiceProcessing();
    info.noiseSuppression = m_session->preferences()->noiseSuppression();

    m_thread = new QThread(this);
    m_thread->setObjectName(QStringLiteral("voice"));
    m_voice = new VoiceSession(info);
    m_voice->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_voice, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    connect(m_voice, &VoiceSession::stateChanged, this, [this](const QString& state) {
        if (state == QLatin1String("connected"))
            setVoiceConnected(true);
    });
    connect(m_voice, &VoiceSession::failed, this, [this](const QString& reason) {
        endCall(tr("The call ended: %1").arg(reason));
    });
    connect(m_voice, &VoiceSession::usersChanged, this, [this](const QStringList& ids) {
        for (const QString& id : ids)
            addParticipant(DiscordUrls::fromId(id));
    });
    connect(m_voice, &VoiceSession::speakingChanged, this, [this](const QString& id, bool speaking) {
        const Snowflake user = DiscordUrls::fromId(id);
        if (speaking)
            m_speaking.insert(user);
        else
            m_speaking.remove(user);
        emit participantsChanged();
    });
    connect(m_voice, &VoiceSession::speakerAvailable, this, [this](bool available) {
        m_speakerAvailable = available;
        m_speaker = true;
        emit speakerChanged();
    });

    m_thread->start();
    QMetaObject::invokeMethod(m_voice, "setMuted", Q_ARG(bool, m_muted));
    QMetaObject::invokeMethod(m_voice, "setDeafened", Q_ARG(bool, m_deafened));
    QMetaObject::invokeMethod(m_voice, "start");
}

void CallManager::setVoiceConnected(bool connected)
{
    m_voiceConnected = connected;
    emit participantsChanged();
    if (connected) {
        if ((!m_direct || !m_participants.isEmpty()) && !m_talkStartedMs) {
            m_talkStartedMs = QDateTime::currentMSecsSinceEpoch();
            m_clock.start();
        }
        setState(Active);
    }
}

void CallManager::stopVoice()
{
    m_voiceConnected = false;
    if (!m_voice)
        return;
    m_voice->disconnect(this);
    QMetaObject::invokeMethod(m_voice, "stop", Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait(2000);
    m_voice = nullptr;
    m_thread = nullptr;
    m_speakerAvailable = false;
    emit speakerChanged();
}

// Phone integration

void CallManager::keepDisplayOn(bool on)
{
    // Ubuntu Touch suspends apps when the screen turns off.
    if (on == m_displayRequested)
        return;
    m_displayRequested = on;
    QDBusInterface* screen = unityScreen();
    if (!screen->isValid())
        return;
    if (on) {
        auto* watcher = new QDBusPendingCallWatcher(screen->asyncCall(QStringLiteral("keepDisplayOn")), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* call) {
            QDBusPendingReply<int> reply = *call;
            if (reply.isValid()) {
                m_displayCookie = uint(reply.value());
                if (!m_displayRequested) // the call ended meanwhile
                    unityScreen()->asyncCall(QStringLiteral("removeDisplayOnRequest"), int(m_displayCookie));
            }
            call->deleteLater();
        });
    } else if (m_displayCookie) {
        screen->asyncCall(QStringLiteral("removeDisplayOnRequest"), int(m_displayCookie));
        m_displayCookie = 0;
    }
}

void CallManager::updateRingtone()
{
    if (m_state == Incoming)
        m_ringtone->play();
    else
        m_ringtone->stop();
}
