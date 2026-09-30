#include "Session.h"

#include "media/GstVideoPlayer.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QStandardPaths>
#include <QUrl>

#include <ctime>
#include <utility>

#include "discord/DiscordInstance.hpp"
#include "discord/config/LocalSettings.hpp"
#include "discord/models/Permissions.hpp"
#include "discord/network/DiscordAPI.hpp"
#include "discord/network/DiscordRequest.hpp"
#include "discord/state/MessageCache.hpp"
#include "discord/state/ProfileCache.hpp"

#include "DiscordUrls.h"
#include "MessageFormatter.h"
#include "RemoteAuth.h"
#include "backend/CoreGlobals.h"
#include "backend/QtFrontend.h"
#include "backend/QtHttpClient.h"
#include "backend/QtWebsocketClient.h"
#include "models/ChannelListModel.h"
#include "models/GuildListModel.h"
#include "models/MessageListModel.h"

namespace {

constexpr int MaxReconnectDelaySeconds = 30;
constexpr qint64 TypingDurationMs = 10000;

// Small gateway payloads (HELLO, heartbeat ACK, RECONNECT, INVALID_SESSION)
// are parsed here to track connection health; dispatches are left to the
// core.
int smallPayloadOpcode(const std::string& payload)
{
    if (payload.size() > 1024)
        return -1;
    try {
        const nlohmann::json j = nlohmann::json::parse(payload);
        if (j.contains("op") && j["op"].is_number_integer())
            return j["op"].get<int>();
    } catch (const std::exception&) {
    }
    return -1;
}

Snowflake guildOfChannel(DiscordInstance* instance, Snowflake channel)
{
    if (instance->m_dmGuild.GetChannel(channel))
        return 0;
    for (Guild& guild : instance->m_guilds) {
        if (guild.GetChannel(channel))
            return guild.m_snowflake;
    }
    return 0;
}

}

Session::Session(QObject* parent)
    : QObject(parent)
{
    m_http = new QtHttpClient(this);
    m_ws = new QtWebsocketClient(this);
    m_frontend = std::make_unique<QtFrontend>(this);
    CoreGlobals::setHttpClient(m_http);
    CoreGlobals::setWebsocketClient(m_ws);
    CoreGlobals::setFrontend(m_frontend.get());

    m_guilds = new GuildListModel(this);
    m_channels = new ChannelListModel(this);
    m_messages = new MessageListModel(this);
    m_unreadDms = new UnreadDmListModel(this);
    m_preferences = new Preferences(this);
    m_emoji = new EmojiPickerModel(this);
    m_offline = new OfflineCache(this);
    m_voiceStates = new VoiceStates(this);
    // Call markers: servers, conversations, voice channels, the open chat.
    connect(m_voiceStates, &VoiceStates::changed, this, [this](Snowflake, Snowflake channel) {
        m_guilds->refreshCalls();
        if (channel)
            m_channels->refreshChannel(channel);
        else
            m_channels->refreshAll();
        updateCurrentCall();
    });
    m_guilds->setVoiceStates(m_voiceStates);
    m_channels->setVoiceStates(m_voiceStates);
    m_callClock.setInterval(1000);
    connect(&m_callClock, &QTimer::timeout, this, &Session::currentCallChanged);
    m_call = new CallManager(this);
    connect(m_call, &CallManager::callFailed, this, &Session::setNotice);
    connect(m_call, &CallManager::notice, this, &Session::setNotice);

    m_qrLogin = new RemoteAuth(m_http->networkAccessManager(), this);
    connect(m_qrLogin, &RemoteAuth::tokenReceived, this, &Session::loginWithToken);

    connect(&m_heartbeatTimer, &QTimer::timeout, this, &Session::heartbeat);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &Session::startGateway);

    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &Session::updatePermissions);
    m_slowmodeTimer.setInterval(1000);
    connect(&m_slowmodeTimer, &QTimer::timeout, this, [this]() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (auto it = m_slowmodeUntil.begin(); it != m_slowmodeUntil.end();)
            it = it.value() <= now ? m_slowmodeUntil.erase(it) : std::next(it);
        if (m_slowmodeUntil.isEmpty())
            m_slowmodeTimer.stop();
        emit slowmodeChanged();
    });

    m_reconnectCountdown.setInterval(1000);
    connect(&m_reconnectCountdown, &QTimer::timeout, this, [this]() {
        if (m_reconnectSeconds > 0) {
            --m_reconnectSeconds;
            emit reconnectSecondsChanged();
        }
        if (m_reconnectSeconds == 0)
            m_reconnectCountdown.stop();
    });

    m_typingTimer.setInterval(1000);
    connect(&m_typingTimer, &QTimer::timeout, this, &Session::updateTypingText);

    m_noticeTimer.setSingleShot(true);
    m_noticeTimer.setInterval(6000);
    connect(&m_noticeTimer, &QTimer::timeout, this, &Session::clearNotice);

    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive)
            markCurrentChannelRead();
    });
}

Session::~Session()
{
    m_qrLogin->stop();
    destroyInstance();
    CoreGlobals::setFrontend(nullptr);
    CoreGlobals::setHttpClient(nullptr);
    CoreGlobals::setWebsocketClient(nullptr);
}

QString Session::configPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/settings.json");
}

bool Session::applicationActive() const
{
    return QGuiApplication::applicationState() == Qt::ApplicationActive;
}

void Session::start()
{
    GetLocalSettings()->Load();

    // DISPORTS_API_URL and DISPORTS_CDN_URL point the client at a test server.
    const QByteArray api = qgetenv("DISPORTS_API_URL");
    if (!api.isEmpty())
        GetLocalSettings()->SetDiscordAPI(api.toStdString());
    const QByteArray cdn = qgetenv("DISPORTS_CDN_URL");
    if (!cdn.isEmpty())
        GetLocalSettings()->SetDiscordCDN(cdn.toStdString());

    const std::string token = GetLocalSettings()->GetToken();
    if (token.empty()) {
        setPhase(LoggedOut);
        return;
    }
    createInstance(token);
    loadCachedState();
    // With cached data there is something to show: the app opens as it does
    // while reconnecting, with the connection banner instead of the splash.
    setPhase(m_cachedStart ? Ready : Connecting);
    startGateway();
}

void Session::loadCachedState()
{
    nlohmann::json ready, supplemental;
    if (!m_instance || !m_offline->loadReady(ready, supplemental))
        return;
    try {
        m_instance->LoadCachedReady(ready, supplemental.is_object() ? &supplemental : nullptr);
    } catch (const std::exception& e) {
        // A cache this build cannot read: start without it.
        qWarning("Offline cache: %s; clearing it", e.what());
        m_offline->clear();
        destroyInstance();
        createInstance(GetLocalSettings()->GetToken());
        return;
    }
    m_cachedStart = true;
    m_messages->setOwnUserId(m_instance->GetUserID());
    emit profileChanged();
    // As after the first real READY: start on direct messages.
    selectDirectMessages();
}

// Session state

void Session::setPhase(Phase phase)
{
    if (m_phase == phase)
        return;
    m_phase = phase;
    emit phaseChanged();
}

void Session::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectedChanged();
}

void Session::setErrorText(const QString& text)
{
    if (m_errorText == text)
        return;
    m_errorText = text;
    emit errorTextChanged();
}

void Session::setNotice(const QString& text)
{
    m_noticeText = text;
    emit noticeTextChanged();
    m_noticeTimer.start();
}

void Session::clearNotice()
{
    if (m_noticeText.isEmpty())
        return;
    m_noticeText.clear();
    emit noticeTextChanged();
}

void Session::setLoadingMessages(bool loading)
{
    if (m_loadingMessages == loading)
        return;
    m_loadingMessages = loading;
    emit loadingMessagesChanged();
}

void Session::setNetworkOnline(bool online)
{
    if (m_networkOnline == online)
        return;
    m_networkOnline = online;
    emit networkOnlineChanged();

    if (!online) {
        // A socket cannot survive losing its network; don't wait for the
        // heartbeat to notice.
        m_reconnectTimer.stop();
        m_reconnectCountdown.stop();
        m_reconnectSeconds = 0;
        emit reconnectSecondsChanged();
        dropGateway();
    } else if (m_instance && !m_connected) {
        m_reconnectDelay = 1;
        startGateway();
    }
}

// Sign in / out

void Session::createInstance(const std::string& token)
{
    m_openAfterReady.clear();
    m_instance = new DiscordInstance(token);
    CoreGlobals::setInstance(m_instance);
    m_fetchedChannels.clear();
    m_requestedMembers.clear();
    m_firstReadyPending = true;
    m_cachedStart = false;
}

void Session::setChatVisible(bool visible)
{
    if (m_chatVisible == visible)
        return;
    m_chatVisible = visible;
    emit chatVisibleChanged();
    if (visible) {
        ensureMessagesLoaded();
        markCurrentChannelRead();
    }
}

void Session::setAutoSelectChannel(bool autoSelect)
{
    if (m_autoSelectChannel == autoSelect)
        return;
    m_autoSelectChannel = autoSelect;
    emit autoSelectChannelChanged();
}

void Session::destroyInstance()
{
    m_heartbeatTimer.stop();
    m_reconnectTimer.stop();
    m_reconnectCountdown.stop();
    m_ws->abortAll();
    m_http->StopAllRequests();

    if (m_call)
        m_call->gatewayLost();
    if (m_voiceStates)
        m_voiceStates->clear();
    if (m_instance) {
        m_instance->CloseGatewaySession();
        CoreGlobals::setInstance(nullptr);
        delete m_instance;
        m_instance = nullptr;
    }
    GetMessageCache()->ClearAllChannels();

    m_messages->setChannel(0, 0);
    m_channels->clear();
    m_guilds->clear();
    m_unreadDms->clear();
    m_fetchedChannels.clear();
    m_requestedMembers.clear();
    m_typingUntil.clear();
    updateTypingText();
    setConnected(false);
    setLoadingMessages(false);
    emit currentGuildChanged();
    emit currentChannelChanged();
    updatePermissions();
    updateCurrentCall();
    emit profileChanged();
}

void Session::loginWithToken(const QString& token)
{
    const std::string trimmed = token.trimmed().toStdString();
    if (trimmed.empty())
        return;

    m_qrLogin->stop();
    setErrorText(QString());
    destroyInstance();

    GetLocalSettings()->SetToken(trimmed);
    GetLocalSettings()->Save();

    // Another account's cache must not show up.
    m_offline->clear();
    createInstance(trimmed);
    setPhase(Connecting);
    m_reconnectDelay = 1;
    startGateway();
}

void Session::logout()
{
    destroyInstance();
    m_offline->clear();
    // The cached pictures belong to this account's contacts and servers.
    QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/pictures")).removeRecursively();
    GetLocalSettings()->SetToken("");
    GetLocalSettings()->Save();
    setPhase(LoggedOut);
}

// Gateway connection

void Session::startGateway()
{
    m_reconnectTimer.stop();
    m_reconnectCountdown.stop();
    if (m_reconnectSeconds != 0) {
        m_reconnectSeconds = 0;
        emit reconnectSecondsChanged();
    }
    if (!m_instance || !m_networkOnline)
        return;

    if (m_instance->HasGatewayURL()) {
        m_instance->StartGatewaySession();
    } else {
        GetHTTPClient()->PerformRequest(false, NetRequest::GET, GetDiscordAPI() + "gateway",
                                        DiscordRequest::GATEWAY, 0);
    }
}

void Session::scheduleReconnect()
{
    if (!m_instance || !m_networkOnline || m_reconnectTimer.isActive())
        return;

    const int delay = m_reconnectDelay;
    m_reconnectDelay = qMin(m_reconnectDelay * 2, MaxReconnectDelaySeconds);
    m_reconnectSeconds = delay;
    emit reconnectSecondsChanged();
    m_reconnectCountdown.start();
    m_reconnectTimer.start(delay * 1000);
}

void Session::dropGateway()
{
    m_heartbeatTimer.stop();
    m_ws->abortAll();
    if (m_instance)
        m_instance->CloseGatewaySession();
    setConnected(false);
}

void Session::reconnect()
{
    m_reconnectDelay = 1;
    dropGateway();
    startGateway();
}

void Session::heartbeat()
{
    if (!m_instance)
        return;
    if (!m_heartbeatAcked) {
        // The last heartbeat was never acknowledged: the connection is dead
        // even though the socket may still look open.
        qWarning("Gateway heartbeat not acknowledged; reconnecting");
        dropGateway();
        scheduleReconnect();
        return;
    }
    m_heartbeatAcked = false;
    m_instance->SendHeartbeat();
}

void Session::coreSetHeartbeatInterval(int ms)
{
    m_heartbeatAcked = true;
    m_heartbeatTimer.start(qMax(ms, 1000));
}

void Session::coreConnecting()
{
    setConnected(false);
}

void Session::coreConnected()
{
    // READY. Everything fetched before may be stale now.
    // Called before the core handles READY, which selects its first server:
    // remember what the cached state had open, to go back to it.
    const bool restore = m_firstReadyPending && m_cachedStart && m_instance;
    const QString keepGuild = restore && m_instance->GetCurrentGuildID() ? DiscordUrls::id(m_instance->GetCurrentGuildID()) : QString();
    const QString keepChannel = restore && m_instance->GetCurrentChannelID() ? DiscordUrls::id(m_instance->GetCurrentChannelID()) : QString();
    m_cachedStart = false;
    m_reconnectDelay = 1;
    m_fetchedChannels.clear();
    m_requestedMembers.clear();
    setErrorText(QString());
    setConnected(true);
    setPhase(Ready);
    m_messages->setOwnUserId(m_instance ? m_instance->GetUserID() : 0);

    // Runs after the core has finished handling READY.
    QTimer::singleShot(0, this, [this, restore, keepGuild, keepChannel]() {
        if (m_firstReadyPending && restore && m_openAfterReady.isEmpty()) {
            // Keep showing what was open with the cached state.
            m_firstReadyPending = false;
            if (!keepChannel.isEmpty())
                openChannel(keepChannel);
            else if (!keepGuild.isEmpty())
                selectGuild(keepGuild);
            else
                selectDirectMessages();
        } else if (m_firstReadyPending) {
            // The core opens the first server, as on a desktop; like the
            // Qt 5 app, start on direct messages instead, unless a channel
            // was asked for in the meantime (e.g. from a notification).
            m_firstReadyPending = false;
            const QString channel = std::exchange(m_openAfterReady, QString());
            if (channel.isEmpty())
                selectDirectMessages();
            else
                openChannel(channel);
        }
        ensureMessagesLoaded();
    });
}

void Session::coreLoginAgain()
{
    m_heartbeatTimer.stop();
    setConnected(false);
    scheduleReconnect();
}

void Session::coreLoggedOut()
{
    // The token was rejected.
    logout();
    setErrorText(tr("Your session has expired. Please sign in again."));
}

void Session::coreSessionClosed(int code)
{
    qWarning("Gateway session closed with code %d", code);
    m_heartbeatTimer.stop();
    setConnected(false);
    scheduleReconnect();
}

void Session::coreGatewayFailed(int gatewayId, const QString& reason)
{
    if (!m_instance)
        return;
    if (gatewayId >= 0 && gatewayId != m_instance->GetGatewayID())
        return;
    qWarning("Gateway connection failed: %s", qPrintable(reason));
    m_heartbeatTimer.stop();
    m_instance->m_gatewayConnId = -1;
    setConnected(false);
    scheduleReconnect();
}

void Session::coreGatewayClosed(int gatewayId, int code)
{
    if (!m_instance || gatewayId != m_instance->GetGatewayID())
        return;
    m_heartbeatTimer.stop();
    m_instance->GatewayClosed(code);
}

void Session::coreGatewayMessage(int gatewayId, const std::string& payload)
{
    if (!m_instance || gatewayId != m_instance->GetGatewayID())
        return;

    switch (smallPayloadOpcode(payload)) {
    case 11: // heartbeat ACK
        m_heartbeatAcked = true;
        return;
    case 7: // RECONNECT
    case 9: // INVALID_SESSION
        // The core does not handle these; start a fresh session.
        dropGateway();
        m_reconnectDelay = 1;
        scheduleReconnect();
        return;
    default:
        break;
    }

    try {
        m_instance->HandleGatewayMessage(payload);
    } catch (const std::exception& e) {
        qWarning("Error handling gateway message: %s", e.what());
    }
}

// Profile and navigation state

QString Session::userId() const
{
    return m_instance ? DiscordUrls::id(m_instance->GetUserID()) : QString();
}

QString Session::username() const
{
    if (!m_instance || !m_instance->GetUserID())
        return QString();
    Profile* profile = m_instance->GetProfile();
    if (!profile)
        return QString();
    return QString::fromStdString(!profile->m_globalName.empty() ? profile->m_globalName : profile->m_name);
}

QString Session::avatarUrl() const
{
    if (!m_instance || !m_instance->GetUserID())
        return QString();
    Profile* profile = m_instance->GetProfile();
    return profile ? DiscordUrls::userAvatar(profile->m_snowflake, profile->m_avatarlnk) : QString();
}

bool Session::inDirectMessages() const
{
    return !m_instance || m_instance->GetCurrentGuildID() == 0;
}

QString Session::currentGuildId() const
{
    return m_instance ? DiscordUrls::id(m_instance->GetCurrentGuildID()) : QString();
}

QString Session::currentGuildName() const
{
    if (inDirectMessages())
        return tr("Direct Messages");
    Guild* guild = m_instance->GetCurrentGuild();
    return guild ? QString::fromStdString(guild->m_name) : QString();
}

QString Session::currentChannelId() const
{
    if (!m_instance || !m_instance->GetCurrentChannelID())
        return QString();
    return DiscordUrls::id(m_instance->GetCurrentChannelID());
}

QString Session::currentChannelName() const
{
    Channel* channel = m_instance ? m_instance->GetCurrentChannel() : nullptr;
    return channel ? ChannelListModel::displayName(*channel) : QString();
}

QString Session::currentChannelTopic() const
{
    Channel* channel = m_instance ? m_instance->GetCurrentChannel() : nullptr;
    return channel ? QString::fromStdString(channel->m_topic) : QString();
}

bool Session::canSendMessages() const
{
    Channel* channel = m_instance ? m_instance->GetCurrentChannel() : nullptr;
    if (!channel)
        return false;
    return channel->IsDM() || channel->HasPermission(PERM_SEND_MESSAGES);
}

bool Session::canManageMessages() const
{
    Channel* channel = currentServerChannel();
    return channel && channel->HasPermission(PERM_MANAGE_MESSAGES);
}

// Permissions

Channel* Session::currentServerChannel() const
{
    Channel* channel = m_instance ? m_instance->GetCurrentChannel() : nullptr;
    return channel && !channel->IsDM() ? channel : nullptr;
}

bool Session::hasPermission(uint64_t permission) const
{
    // Direct messages and groups: no permissions to check.
    Channel* channel = m_instance ? m_instance->GetCurrentChannel() : nullptr;
    if (!channel)
        return false;
    return channel->IsDM() || channel->HasPermission(permission);
}

bool Session::canAddReactions() const
{
    return hasPermission(PERM_ADD_REACTIONS) && hasPermission(PERM_READ_MESSAGE_HISTORY);
}

bool Session::canUseReactions() const
{
    // Timeouts take reactions away too (see Channel::ComputePermissionOverwrites).
    return hasPermission(PERM_READ_MESSAGE_HISTORY) && timeoutUntilMs() == 0;
}

bool Session::canReadHistory() const
{
    return hasPermission(PERM_READ_MESSAGE_HISTORY);
}

qint64 Session::timeoutUntilMs() const
{
    Channel* channel = currentServerChannel();
    if (!channel || !m_instance)
        return 0;
    Profile* me = m_instance->GetProfile();
    if (!me)
        return 0;
    auto it = me->m_guildMembers.find(channel->m_parentGuild);
    if (it == me->m_guildMembers.end() || it->second.m_timeoutUntil <= time(nullptr))
        return 0;
    // Owners and administrators cannot be timed out.
    if (channel->HasPermission(PERM_ADMINISTRATOR))
        return 0;
    return qint64(it->second.m_timeoutUntil) * 1000;
}

QString Session::timeoutText() const
{
    const qint64 until = timeoutUntilMs();
    if (!until)
        return QString();
    const QDateTime end = QDateTime::fromMSecsSinceEpoch(until);
    const QString when = end.date() == QDate::currentDate()
        ? QLocale().toString(end.time(), QLocale::ShortFormat)
        : QLocale().toString(end, QLocale::ShortFormat);
    return tr("You're timed out until %1").arg(when);
}

int Session::slowmodeSeconds() const
{
    Channel* channel = currentServerChannel();
    // Moderators are not slowed down.
    if (!channel || channel->HasPermission(PERM_MANAGE_MESSAGES) || channel->HasPermission(PERM_MANAGE_CHANNELS))
        return 0;
    return channel->m_slowmodeSeconds;
}

int Session::slowmodeRemaining() const
{
    const qint64 until = m_slowmodeUntil.value(m_messages->channel());
    const qint64 left = until - QDateTime::currentMSecsSinceEpoch();
    return left > 0 ? int((left + 999) / 1000) : 0;
}

bool Session::canAttachFiles() const
{
    return canSendMessages() && hasPermission(PERM_ATTACH_FILES);
}

bool Session::canMentionEveryone() const
{
    // Groups and DMs have no @everyone.
    return currentServerChannel() && hasPermission(PERM_MENTION_EVERYONE);
}

void Session::updatePermissions()
{
    // A timeout ends by itself: look again then.
    const qint64 until = timeoutUntilMs();
    if (until)
        m_timeoutTimer.start(int(qBound(qint64(1000), until - QDateTime::currentMSecsSinceEpoch() + 500, qint64(3600000))));
    else
        m_timeoutTimer.stop();
    emit permissionsChanged();
    emit slowmodeChanged();
}

QVariantMap Session::channelInfo(const QString& channelId) const
{
    QVariantMap info;
    Channel* channel = m_instance ? m_instance->GetChannel(DiscordUrls::fromId(channelId)) : nullptr;
    if (!channel)
        return info;

    auto str = [](const std::string& s) { return QString::fromStdString(s); };
    QString kind = QStringLiteral("channel"), typeName;
    switch (channel->m_channelType) {
    case Channel::DM:         kind = QStringLiteral("dm"); typeName = tr("Direct message"); break;
    case Channel::GROUPDM:    kind = QStringLiteral("group"); typeName = tr("Group"); break;
    case Channel::VOICE:      typeName = tr("Voice channel"); break;
    case Channel::STAGEVOICE: typeName = tr("Stage channel"); break;
    case Channel::NEWS:       typeName = tr("Announcement channel"); break;
    case Channel::FORUM:      typeName = tr("Forum"); break;
    case Channel::MEDIA:      typeName = tr("Media channel"); break;
    case Channel::NEWSTHREAD:
    case Channel::PUBTHREAD:
    case Channel::PRIVTHREAD: typeName = tr("Thread"); break;
    default:                  typeName = tr("Text channel"); break;
    }

    info.insert(QStringLiteral("id"), channelId);
    info.insert(QStringLiteral("kind"), kind);
    info.insert(QStringLiteral("typeName"), typeName);
    info.insert(QStringLiteral("name"), ChannelListModel::displayName(*channel));
    info.insert(QStringLiteral("topic"), str(channel->m_topic));
    info.insert(QStringLiteral("nsfw"), channel->m_bNSFW);
    info.insert(QStringLiteral("iconUrl"), ChannelListModel::iconUrl(*channel));

    if (Channel* category = channel->m_parentCateg ? m_instance->GetChannel(channel->m_parentCateg) : nullptr)
        info.insert(QStringLiteral("category"), str(category->m_name));
    if (Guild* guild = channel->m_parentGuild ? m_instance->GetGuild(channel->m_parentGuild) : nullptr)
        info.insert(QStringLiteral("server"), str(guild->m_name));

    if (channel->IsDM()) {
        QVariantList members;
        for (Snowflake id : channel->m_recipients) {
            Profile* profile = GetProfileCache()->LookupProfile(id, "", "", "", false);
            const QString username = profile ? str(profile->m_name) : QString();
            members.append(QVariantMap{
                {QStringLiteral("id"), DiscordUrls::id(id)},
                {QStringLiteral("name"), profile && !profile->m_globalName.empty() ? str(profile->m_globalName) : username},
                {QStringLiteral("username"), username},
                {QStringLiteral("avatarUrl"), DiscordUrls::userAvatar(id, profile ? profile->m_avatarlnk : std::string())},
                {QStringLiteral("blocked"), m_instance->IsUserBlocked(id)},
            });
        }
        info.insert(QStringLiteral("members"), members);
    }
    return info;
}

bool Session::editMessage(const QString& messageId, const QString& text)
{
    const QString content = text.trimmed();
    if (!m_instance || !m_connected || content.isEmpty())
        return false;
    if (!m_instance->EditMessageInCurrentChannel(content.toStdString(), DiscordUrls::fromId(messageId))) {
        setNotice(tr("The message could not be edited."));
        return false;
    }
    return true;
}

void Session::deleteMessage(const QString& messageId)
{
    if (!m_instance || !m_connected || !m_messages->channel())
        return;
    m_instance->RequestDeleteMessage(m_messages->channel(), DiscordUrls::fromId(messageId));
}

void Session::selectDirectMessages()
{
    if (!m_instance)
        return;
    m_instance->OnSelectGuild(0);
    afterGuildSelected();
}

void Session::selectGuild(const QString& guildId)
{
    if (!m_instance)
        return;
    m_instance->OnSelectGuild(DiscordUrls::fromId(guildId));
    afterGuildSelected();
}

void Session::afterGuildSelected()
{
    // The core opens the server's first channel. On a phone the channel
    // list is shown instead, so undo that.
    if (!m_autoSelectChannel && m_instance->GetCurrentChannelID())
        m_instance->OnSelectChannel(0);
}

void Session::openChannel(const QString& channelId)
{
    if (!m_instance)
        return;
    if (m_firstReadyPending && !m_cachedStart) {
        // The core is still setting up the session; it would override this.
        m_openAfterReady = channelId;
        return;
    }
    const Snowflake channel = DiscordUrls::fromId(channelId);
    const Snowflake guild = guildOfChannel(m_instance, channel);

    if (guild != m_instance->GetCurrentGuildID())
        m_instance->OnSelectGuild(guild, channel);
    else if (channel != m_instance->GetCurrentChannelID())
        m_instance->OnSelectChannel(channel);

    ensureMessagesLoaded();
    markCurrentChannelRead();
}

void Session::coreSelectedGuildChanged()
{
    m_channels->reload();
    m_emoji->reloadServerEmoji();
    emit currentGuildChanged();
}

void Session::coreSelectedChannelChanged()
{
    if (!m_instance)
        return;
    m_instance->HandledChannelSwitch();
    m_messages->setChannel(m_instance->GetCurrentGuildID(), m_instance->GetCurrentChannelID());
    m_typingUntil.clear();
    updateTypingText();
    emit currentChannelChanged();
    updatePermissions();
    updateCurrentCall();
    ensureMessagesLoaded();
}

void Session::coreChannelListChanged()
{
    // Also after role, member and overwrite changes.
    m_channels->reload();
    refreshUnread();
    updatePermissions();
}

void Session::coreChannelAcknowledged(Snowflake channel)
{
    m_channels->refreshChannel(channel);
    refreshUnread();
}

void Session::refreshUnread()
{
    m_guilds->refreshUnread();
    m_unreadDms->reload();
}

void Session::coreGuildListChanged()
{
    m_guilds->reload();
    m_unreadDms->reload();
    m_channels->reload();
    if (m_instance)
        m_messages->setOwnUserId(m_instance->GetUserID());
    emit currentGuildChanged();
    emit currentChannelChanged();
    updatePermissions();
    updateCurrentCall();
    emit profileChanged();
}

void Session::coreProfileChanged()
{
    if (m_instance)
        m_messages->setOwnUserId(m_instance->GetUserID());
    emit profileChanged();
}

void Session::coreUserChanged(Snowflake)
{
    // Names, avatars and presence shown in the lists.
    m_channels->refreshAll();
    if (m_messages->channel())
        m_messages->sync();
}

// Messages

void Session::ensureMessagesLoaded()
{
    if (!m_instance)
        return;
    const Snowflake channel = m_instance->GetCurrentChannelID();

    // Nothing loaded yet: show the offline cache's messages meanwhile (the
    // first fetch replaces them).
    if (channel && !GetMessageCache()->HasMessages(channel)) {
        nlohmann::json cached = m_offline->messages(channel);
        if (!cached.empty()) {
            Channel* info = m_instance->GetChannel(channel);
            GetMessageCache()->LoadCachedMessages(channel, cached, info ? info->GetTypeSymbol() + info->m_name : std::string());
            m_messages->sync();
        }
    }

    if (!m_connected || !m_chatVisible)
        return;
    if (!channel || m_fetchedChannels.contains(channel))
        return;
    // Without "Read Message History" Discord sends nothing back.
    if (!canReadHistory())
        return;

    m_fetchedChannels.insert(channel);
    setLoadingMessages(true);
    // The core only clears its "initial load in progress" flag on a channel
    // switch; clear it here so a failed load can be retried.
    m_instance->HandledChannelSwitch();
    m_instance->RequestMessages(channel, ScrollDir::BEFORE, 0, 0);
}

void Session::loadOlderMessages()
{
    if (!m_instance || m_loadingMessages || !canReadHistory())
        return;
    const MessagePtr gap = m_messages->olderGap();
    if (!gap)
        return;
    setLoadingMessages(true);
    // Allow retrying an anchor whose earlier request failed.
    m_instance->m_messageRequestsInProgress.erase(gap->m_anchor);
    m_instance->RequestMessages(m_messages->channel(), ScrollDir::BEFORE, gap->m_anchor, gap->m_snowflake);
}

void Session::coreMessagesRefreshed()
{
    setLoadingMessages(false);
    m_messages->sync();
    requestMissingMembers();
    markCurrentChannelRead();
}

void Session::markCurrentChannelRead()
{
    if (!m_instance || !m_connected || !m_chatVisible || !applicationActive())
        return;
    Channel* channel = m_instance->GetCurrentChannel();
    const Snowflake newest = m_messages->newestMessageId();
    if (!channel || !newest || m_messages->channel() != channel->m_snowflake)
        return;
    if (channel->m_lastViewedMsg >= newest && channel->m_mentionCount == 0)
        return;

    m_instance->RequestAcknowledgeMessages(channel->m_snowflake, newest);
    // Clear the unread marker right away; the gateway confirms it later.
    channel->m_lastViewedMsg = newest;
    channel->m_mentionCount = 0;
    coreChannelAcknowledged(channel->m_snowflake);
}

void Session::sendMessage(const QString& text, const QString& replyToId)
{
    if (!m_instance)
        return;
    const QString content = text.trimmed();
    if (content.isEmpty())
        return;

    if (const int wait = slowmodeRemaining()) {
        setNotice(tr("Slowmode is on: you can send another message in %n second(s).", nullptr, wait));
        return;
    }

    Snowflake tempId = 0;
    const Snowflake replyTo = replyToId.isEmpty() ? 0 : DiscordUrls::fromId(replyToId);
    if (!m_instance->SendMessageToCurrentChannel(content.toStdString(), tempId, replyTo, true)) {
        setNotice(tr("You can't send messages in this channel."));
        return;
    }
    if (const int seconds = slowmodeSeconds()) {
        m_slowmodeUntil.insert(m_instance->GetCurrentChannelID(), QDateTime::currentMSecsSinceEpoch() + seconds * 1000);
        m_slowmodeTimer.start();
        emit slowmodeChanged();
    }

    // Show the message right away; the real one replaces it by nonce.
    Message pending;
    pending.m_snowflake = tempId;
    pending.m_type = MessageType::SENDING_MESSAGE;
    pending.m_message = content.toStdString();
    if (Profile* profile = m_instance->GetProfile()) {
        pending.m_author_snowflake = profile->m_snowflake;
        pending.m_author = !profile->m_globalName.empty() ? profile->m_globalName : profile->m_name;
        pending.m_avatar = profile->m_avatarlnk;
    }
    pending.SetTime(time(nullptr));
    GetMessageCache()->AddMessage(m_instance->GetCurrentChannelID(), pending);
    m_messages->sync();
}

void Session::coreFailedToSend(Snowflake channel, Snowflake nonce)
{
    GetMessageCache()->DeleteMessage(channel, nonce);
    if (channel == m_messages->channel())
        m_messages->sync();
}

void Session::coreMessageAdded(Snowflake channel, const Message& msg)
{
    if (channel == m_messages->channel()) {
        m_messages->sync();
        if (m_typingUntil.remove(msg.m_author_snowflake))
            updateTypingText();
        markCurrentChannelRead();
    }

    if (inDirectMessages() && m_instance && m_instance->m_dmGuild.GetChannel(channel))
        m_channels->reload(); // conversations are ordered by activity
    else
        m_channels->refreshChannel(channel);
    refreshUnread();
}

void Session::coreMessageUpdated(Snowflake channel, const Message&)
{
    if (channel == m_messages->channel())
        m_messages->sync();
}

void Session::coreMessageDeleted(Snowflake)
{
    if (m_messages->channel())
        m_messages->sync();
}

void Session::addReaction(const QString& messageId, const QString& emoji)
{
    if (!m_instance || !m_connected || emoji.isEmpty() || !m_messages->channel() || !canUseReactions())
        return;
    m_instance->RequestAddReaction(m_messages->channel(), DiscordUrls::fromId(messageId), emoji.toStdString());
}

void Session::toggleReaction(const QString& messageId, const QString& emoji, bool reacted)
{
    if (!m_instance || !m_connected || emoji.isEmpty() || !m_messages->channel() || !canUseReactions())
        return;
    const Snowflake message = DiscordUrls::fromId(messageId);
    if (reacted)
        m_instance->RequestRemoveReaction(m_messages->channel(), message, emoji.toStdString());
    else
        m_instance->RequestAddReaction(m_messages->channel(), message, emoji.toStdString());
}

void Session::votePoll(const QString& messageId, const QVariantList& answerIds)
{
    if (!m_instance || !m_connected || !m_messages->channel())
        return;
    std::vector<int> ids;
    for (const QVariant& id : answerIds)
        ids.push_back(id.toInt());
    m_instance->RequestPollVote(m_messages->channel(), DiscordUrls::fromId(messageId), ids);
}

bool Session::videoPlaybackAvailable()
{
    return GstVideoPlayer::isAvailable();
}

void Session::notifyTyping()
{
    if (m_instance && m_connected)
        m_instance->Typing();
}

void Session::coreTyping(Snowflake user, Snowflake, Snowflake channel)
{
    if (!m_instance || channel != m_instance->GetCurrentChannelID() || user == m_instance->GetUserID())
        return;
    m_typingUntil.insert(user, QDateTime::currentMSecsSinceEpoch() + TypingDurationMs);
    updateTypingText();
}

void Session::updateTypingText()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_typingUntil.begin(); it != m_typingUntil.end();) {
        if (it.value() <= now)
            it = m_typingUntil.erase(it);
        else
            ++it;
    }

    QStringList names;
    if (m_instance) {
        const Snowflake guild = m_instance->GetCurrentGuildID();
        for (auto it = m_typingUntil.constBegin(); it != m_typingUntil.constEnd(); ++it)
            names.append(MessageFormatter::displayName(it.key(), guild, QStringLiteral("Someone")));
    }
    names.sort();

    QString text;
    if (names.size() == 1)
        text = tr("%1 is typing…").arg(names[0]);
    else if (names.size() == 2)
        text = tr("%1 and %2 are typing…").arg(names[0], names[1]);
    else if (names.size() > 2)
        text = tr("Several people are typing…");

    if (names.isEmpty())
        m_typingTimer.stop();
    else if (!m_typingTimer.isActive())
        m_typingTimer.start();

    if (text != m_typingText) {
        m_typingText = text;
        emit typingTextChanged();
    }
}

void Session::coreError(const QString& message)
{
    qWarning("Discord: %s", qPrintable(message));
    if (m_loadingMessages) {
        setLoadingMessages(false);
        if (m_instance)
            m_fetchedChannels.remove(m_instance->GetCurrentChannelID());
    }
    // Error texts from the core can be long (they include the response);
    // keep the first line for the UI.
    setNotice(message.section(QLatin1Char('\n'), 0, 0));
}

// Attachments

bool Session::sendAttachment(const QString& fileUrl, const QString& text)
{
    if (!m_instance || !m_connected)
        return false;
    if (m_uploading) {
        setNotice(tr("Wait for the file being sent to finish."));
        return false;
    }
    if (!canAttachFiles()) {
        setNotice(tr("You can't attach files in this channel."));
        return false;
    }
    if (const int wait = slowmodeRemaining()) {
        setNotice(tr("Slowmode is on: you can send another message in %n second(s).", nullptr, wait));
        return false;
    }

    const QUrl url(fileUrl);
    QFile file(url.isLocalFile() ? url.toLocalFile() : fileUrl);
    // Discord's largest upload (with Nitro) is 500 MB.
    constexpr qint64 MaxSize = 500ll * 1024 * 1024;
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxSize) {
        setNotice(file.size() > MaxSize ? tr("%1 is too large to send.").arg(QFileInfo(file).fileName())
                                         : tr("%1 could not be read.").arg(QFileInfo(file).fileName()));
        return false;
    }
    QByteArray data = file.readAll();
    const QString name = QFileInfo(file).fileName();
    const QString content = text.trimmed();

    Snowflake tempId = 0;
    if (!m_instance->SendMessageAndAttachmentToCurrentChannel(content.toStdString(), tempId,
            reinterpret_cast<uint8_t*>(data.data()), size_t(data.size()), name.toStdString())) {
        setNotice(tr("You can't attach files in this channel."));
        return false;
    }
    m_uploading = true;
    m_uploadCancelled = false;
    m_uploadName = name;
    m_uploadProgress = 0;
    emit uploadChanged();

    if (const int seconds = slowmodeSeconds()) {
        m_slowmodeUntil.insert(m_instance->GetCurrentChannelID(), QDateTime::currentMSecsSinceEpoch() + seconds * 1000);
        m_slowmodeTimer.start();
        emit slowmodeChanged();
    }

    // Shown until the real message arrives (by nonce), like a text message.
    Message pending;
    pending.m_snowflake = tempId;
    pending.m_type = MessageType::SENDING_MESSAGE;
    pending.m_message = (content.isEmpty() ? name : content + QStringLiteral("\n") + name).toStdString();
    if (Profile* profile = m_instance->GetProfile()) {
        pending.m_author_snowflake = profile->m_snowflake;
        pending.m_author = !profile->m_globalName.empty() ? profile->m_globalName : profile->m_name;
        pending.m_avatar = profile->m_avatarlnk;
    }
    pending.SetTime(time(nullptr));
    GetMessageCache()->AddMessage(m_instance->GetCurrentChannelID(), pending);
    m_messages->sync();
    return true;
}

void Session::cancelUpload()
{
    if (m_uploading)
        m_uploadCancelled = true;
}

void Session::coreUploadStarted(Snowflake, const QString& name)
{
    m_uploadName = name;
    emit uploadChanged();
}

bool Session::coreUploadProgress(Snowflake, size_t offset, size_t length)
{
    if (length)
        m_uploadProgress = qreal(offset) / qreal(length);
    emit uploadChanged();
    return m_uploadCancelled;
}

void Session::coreUploadStopped(Snowflake)
{
    m_uploading = false;
    m_uploadProgress = 0;
    emit uploadChanged();
}

void Session::coreUploadFailed(const QString& name, int error)
{
    const bool cancelled = m_uploadCancelled;
    m_uploading = false;
    m_uploadCancelled = false;
    m_uploadProgress = 0;
    emit uploadChanged();
    if (cancelled)
        return;
    // Discord answers 400 / 413 for files over this server's limit.
    if (error == 400 || error == 413)
        setNotice(tr("%1 could not be sent: it may be larger than this server allows.").arg(name));
    else
        setNotice(tr("%1 could not be sent (error %2).").arg(name).arg(error));
}

// Mentions

QVariantList Session::mentionSuggestions(const QString& word) const
{
    QVariantList result;
    Channel* channel = m_instance ? m_instance->GetCurrentChannel() : nullptr;
    if (!channel || word.isEmpty())
        return result;
    const QChar trigger = word.at(0);
    const QString query = word.mid(1).toLower();
    Guild* guild = channel->IsDM() ? nullptr : m_instance->GetGuild(channel->m_parentGuild);

    // Starting with the query first, then containing it; by name.
    struct Candidate { int rank; QVariantMap item; };
    std::vector<Candidate> found;
    auto rankOf = [&query](std::initializer_list<QString> names) {
        int best = -1;
        for (const QString& name : names) {
            const QString lower = name.toLower();
            if (lower.isEmpty())
                continue;
            if (lower.startsWith(query))
                return 0;
            if (lower.contains(query))
                best = 1;
        }
        return best;
    };
    auto str = [](const std::string& s) { return QString::fromStdString(s); };

    if (trigger == QLatin1Char('@')) {
        std::vector<Snowflake> people;
        if (guild)
            people.assign(guild->m_knownMembers.begin(), guild->m_knownMembers.end());
        else
            people = channel->m_recipients;
        for (Snowflake id : people) {
            Profile* profile = GetProfileCache()->LookupProfile(id, "", "", "", false);
            if (!profile || profile->GetUsername().empty())
                continue;
            QString nick;
            if (guild) {
                auto member = profile->m_guildMembers.find(guild->m_snowflake);
                if (member != profile->m_guildMembers.end())
                    nick = str(member->second.m_nick);
            }
            const QString username = str(profile->GetUsername());
            const QString global = str(profile->m_globalName);
            const int rank = rankOf({nick, global, username});
            if (rank < 0)
                continue;
            found.push_back({rank, QVariantMap{
                {QStringLiteral("kind"), QStringLiteral("user")},
                {QStringLiteral("label"), !nick.isEmpty() ? nick : !global.isEmpty() ? global : username},
                {QStringLiteral("detail"), QStringLiteral("@") + username},
                {QStringLiteral("insert"), QStringLiteral("@") + username},
                {QStringLiteral("avatarUrl"), DiscordUrls::userAvatar(id, profile->m_avatarlnk)},
            }});
        }
        if (guild && canMentionEveryone()) {
            const std::pair<QString, QString> everyone[] = {
                {QStringLiteral("everyone"), tr("Notify everyone who can see this channel")},
                {QStringLiteral("here"), tr("Notify everyone online who can see this channel")},
            };
            for (const auto& [name, detail] : everyone) {
                if (!name.startsWith(query))
                    continue;
                found.push_back({0, QVariantMap{
                    {QStringLiteral("kind"), QStringLiteral("everyone")},
                    {QStringLiteral("label"), QStringLiteral("@") + name},
                    {QStringLiteral("detail"), detail},
                    {QStringLiteral("insert"), QStringLiteral("@") + name},
                }});
            }
        }
        if (guild) {
            for (const auto& [id, role] : guild->m_roles) {
                // The @everyone role has the server's id.
                if (id == guild->m_snowflake || (!role.m_bMentionable && !canMentionEveryone()))
                    continue;
                const QString name = str(role.m_name);
                const int rank = rankOf({name});
                if (rank < 0)
                    continue;
                found.push_back({rank, QVariantMap{
                    {QStringLiteral("kind"), QStringLiteral("role")},
                    {QStringLiteral("label"), QStringLiteral("@") + name},
                    {QStringLiteral("detail"), tr("Role")},
                    {QStringLiteral("insert"), QStringLiteral("@") + name},
                    {QStringLiteral("color"), role.m_colorOriginal
                        ? QColor(QRgb(role.m_colorOriginal)).name() : QString()},
                }});
            }
        }
    } else if (trigger == QLatin1Char('#') && guild) {
        for (Channel& other : guild->m_channels) {
            if (other.IsCategory() || !other.HasPermission(PERM_VIEW_CHANNEL))
                continue;
            const QString name = str(other.m_name);
            const int rank = rankOf({name});
            if (rank < 0)
                continue;
            Channel* category = other.m_parentCateg ? guild->GetChannel(other.m_parentCateg) : nullptr;
            found.push_back({rank, QVariantMap{
                {QStringLiteral("kind"), QStringLiteral("channel")},
                {QStringLiteral("label"), QStringLiteral("#") + name},
                {QStringLiteral("detail"), category && category != &other ? str(category->m_name) : QString()},
                {QStringLiteral("insert"), QStringLiteral("#") + name},
            }});
        }
    }

    std::stable_sort(found.begin(), found.end(), [](const Candidate& a, const Candidate& b) {
        if (a.rank != b.rank)
            return a.rank < b.rank;
        return a.item.value(QStringLiteral("label")).toString().compare(
                   b.item.value(QStringLiteral("label")).toString(), Qt::CaseInsensitive) < 0;
    });
    constexpr size_t Limit = 8;
    for (size_t i = 0; i < found.size() && i < Limit; ++i)
        result.append(found[i].item);
    return result;
}

void Session::searchMembers(const QString& query)
{
    Channel* channel = currentServerChannel();
    if (!channel || !m_connected || query.trimmed().isEmpty())
        return;
    m_instance->RequestGuildMembers(channel->m_parentGuild, query.trimmed().toStdString(), false, 10);
}

void Session::requestMissingMembers()
{
    const Snowflake guild = m_messages->guild();
    if (!m_instance || !m_connected || !guild)
        return;
    std::set<Snowflake> wanted;
    for (Snowflake user : m_messages->unknownMembers()) {
        if (!m_requestedMembers.contains(qMakePair(guild, user))) {
            m_requestedMembers.insert(qMakePair(guild, user));
            wanted.insert(user);
        }
    }
    // Discord takes up to 100 ids a request.
    std::set<Snowflake> batch;
    for (Snowflake user : wanted) {
        batch.insert(user);
        if (batch.size() == 100) {
            m_instance->RequestGuildMembers(guild, batch);
            batch.clear();
        }
    }
    if (!batch.empty())
        m_instance->RequestGuildMembers(guild, batch);
}

void Session::coreMembersChanged()
{
    // Nicknames: messages, replies, mentions, who is typing; also the
    // names of people in voice channels.
    m_messages->refreshNames();
    updateTypingText();
    m_channels->refreshAll();
    emit membersChanged();
}

// Calls in the open conversation

bool Session::currentChannelHasCall() const
{
    return m_instance && m_voiceStates->hasCall(m_instance->GetCurrentChannelID());
}

QString Session::currentCallElapsed() const
{
    const qint64 started = m_instance ? m_voiceStates->callStartedMs(m_instance->GetCurrentChannelID()) : 0;
    if (!started || !currentChannelHasCall())
        return QString();
    const qint64 seconds = qMax<qint64>(0, (QDateTime::currentMSecsSinceEpoch() - started) / 1000);
    const QString mmss = QStringLiteral("%1:%2").arg(seconds / 60 % 60, 2, 10, QLatin1Char('0'))
                                                .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    return seconds >= 3600 ? QStringLiteral("%1:%2").arg(seconds / 3600).arg(mmss) : mmss;
}

void Session::updateCurrentCall()
{
    if (currentChannelHasCall())
        m_callClock.start();
    else
        m_callClock.stop();
    emit currentCallChanged();
}
