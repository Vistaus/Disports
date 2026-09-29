#include "Session.h"

#include <QMediaPlayer>

#include <QDateTime>
#include <QGuiApplication>
#include <QStandardPaths>

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

    m_qrLogin = new RemoteAuth(m_http->networkAccessManager(), this);
    connect(m_qrLogin, &RemoteAuth::tokenReceived, this, &Session::loginWithToken);

    connect(&m_heartbeatTimer, &QTimer::timeout, this, &Session::heartbeat);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &Session::startGateway);

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
    setPhase(Connecting);
    startGateway();
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
    m_firstReadyPending = true;
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
    m_typingUntil.clear();
    updateTypingText();
    setConnected(false);
    setLoadingMessages(false);
    emit currentGuildChanged();
    emit currentChannelChanged();
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

    createInstance(trimmed);
    setPhase(Connecting);
    m_reconnectDelay = 1;
    startGateway();
}

void Session::logout()
{
    destroyInstance();
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
    m_reconnectDelay = 1;
    m_fetchedChannels.clear();
    setErrorText(QString());
    setConnected(true);
    setPhase(Ready);
    m_messages->setOwnUserId(m_instance ? m_instance->GetUserID() : 0);

    // Runs after the core has finished handling READY.
    QTimer::singleShot(0, this, [this]() {
        if (m_firstReadyPending) {
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
    if (m_firstReadyPending) {
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
    ensureMessagesLoaded();
}

void Session::coreChannelListChanged()
{
    m_channels->reload();
    refreshUnread();
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
    if (!m_instance || !m_connected || !m_chatVisible)
        return;
    const Snowflake channel = m_instance->GetCurrentChannelID();
    if (!channel || m_fetchedChannels.contains(channel))
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
    if (!m_instance || m_loadingMessages)
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

    Snowflake tempId = 0;
    const Snowflake replyTo = replyToId.isEmpty() ? 0 : DiscordUrls::fromId(replyToId);
    if (!m_instance->SendMessageToCurrentChannel(content.toStdString(), tempId, replyTo, true)) {
        setNotice(tr("You can't send messages in this channel."));
        return;
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
    if (!m_instance || !m_connected || emoji.isEmpty() || !m_messages->channel())
        return;
    m_instance->RequestAddReaction(m_messages->channel(), DiscordUrls::fromId(messageId), emoji.toStdString());
}

void Session::toggleReaction(const QString& messageId, const QString& emoji, bool reacted)
{
    if (!m_instance || !m_connected || emoji.isEmpty() || !m_messages->channel())
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
    if (m_videoAvailable < 0) {
        QMediaPlayer probe;
        m_videoAvailable = probe.isAvailable() ? 1 : 0;
    }
    return m_videoAvailable == 1;
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
            names.append(QString::fromStdString(m_instance->LookupUserNameGlobally(it.key(), guild)));
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
