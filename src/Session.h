#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <memory>

#include "discord/models/Snowflake.hpp"

// Complete types: moc needs them for the QObject* properties below.
#include "Preferences.h"
#include "RemoteAuth.h"
#include "models/ChannelListModel.h"
#include "models/EmojiPickerModel.h"
#include "OfflineCache.h"
#include "models/GuildListModel.h"
#include "models/MessageListModel.h"
#include "models/UnreadDmListModel.h"

class DiscordInstance;
class Message;
class QtFrontend;
class QtHttpClient;
class QtWebsocketClient;

// The QML-facing session: owns Discord Messenger's core (DiscordInstance),
// the Qt transports and the list models, and exposes navigation and chat
// actions to QML. Everything runs on the main thread.
class Session : public QObject
{
    Q_OBJECT

    Q_PROPERTY(Phase phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool networkOnline READ networkOnline WRITE setNetworkOnline NOTIFY networkOnlineChanged)
    Q_PROPERTY(int reconnectSeconds READ reconnectSeconds NOTIFY reconnectSecondsChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString noticeText READ noticeText NOTIFY noticeTextChanged)

    Q_PROPERTY(QString userId READ userId NOTIFY profileChanged)
    Q_PROPERTY(QString username READ username NOTIFY profileChanged)
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY profileChanged)

    Q_PROPERTY(bool inDirectMessages READ inDirectMessages NOTIFY currentGuildChanged)
    Q_PROPERTY(QString currentGuildId READ currentGuildId NOTIFY currentGuildChanged)
    Q_PROPERTY(QString currentGuildName READ currentGuildName NOTIFY currentGuildChanged)

    Q_PROPERTY(QString currentChannelId READ currentChannelId NOTIFY currentChannelChanged)
    Q_PROPERTY(QString currentChannelName READ currentChannelName NOTIFY currentChannelChanged)
    Q_PROPERTY(QString currentChannelTopic READ currentChannelTopic NOTIFY currentChannelChanged)
    Q_PROPERTY(bool canSendMessages READ canSendMessages NOTIFY currentChannelChanged)
    // May delete other people's messages here (moderators in servers).
    Q_PROPERTY(bool canManageMessages READ canManageMessages NOTIFY currentChannelChanged)
    Q_PROPERTY(QString typingText READ typingText NOTIFY typingTextChanged)
    Q_PROPERTY(bool loadingMessages READ loadingMessages NOTIFY loadingMessagesChanged)

    // Set by the UI: a chat is on screen (fetch history, mark read), and
    // whether picking a server should also open its first channel (wide
    // layout) or only show the channel list (phone).
    Q_PROPERTY(bool chatVisible READ chatVisible WRITE setChatVisible NOTIFY chatVisibleChanged)
    Q_PROPERTY(bool autoSelectChannel READ autoSelectChannel WRITE setAutoSelectChannel NOTIFY autoSelectChannelChanged)

    Q_PROPERTY(GuildListModel* guilds READ guilds CONSTANT)
    Q_PROPERTY(ChannelListModel* channels READ channels CONSTANT)
    Q_PROPERTY(MessageListModel* messages READ messages CONSTANT)
    Q_PROPERTY(UnreadDmListModel* unreadDirectMessages READ unreadDirectMessages CONSTANT)
    Q_PROPERTY(Preferences* preferences READ preferences CONSTANT)
    Q_PROPERTY(EmojiPickerModel* emoji READ emoji CONSTANT)
    Q_PROPERTY(RemoteAuth* qrLogin READ qrLogin CONSTANT)

public:
    enum Phase {
        Starting,   // loading settings
        LoggedOut,  // show the login page
        Connecting, // signed in, waiting for READY the first time
        Ready,      // READY received at least once; UI usable
    };
    Q_ENUM(Phase)

    explicit Session(QObject* parent = nullptr);
    ~Session() override;

    // Loads the saved settings and signs in if a token is stored.
    void start();

    Phase phase() const { return m_phase; }
    bool connected() const { return m_connected; }
    bool networkOnline() const { return m_networkOnline; }
    void setNetworkOnline(bool online);
    int reconnectSeconds() const { return m_reconnectSeconds; }
    QString errorText() const { return m_errorText; }
    QString noticeText() const { return m_noticeText; }

    QString userId() const;
    QString username() const;
    QString avatarUrl() const;

    bool inDirectMessages() const;
    QString currentGuildId() const;
    QString currentGuildName() const;
    QString currentChannelId() const;
    QString currentChannelName() const;
    QString currentChannelTopic() const;
    bool canManageMessages() const;
    bool canSendMessages() const;
    QString typingText() const { return m_typingText; }
    bool loadingMessages() const { return m_loadingMessages; }
    bool chatVisible() const { return m_chatVisible; }
    void setChatVisible(bool visible);
    bool autoSelectChannel() const { return m_autoSelectChannel; }
    void setAutoSelectChannel(bool autoSelect);

    GuildListModel* guilds() const { return m_guilds; }
    ChannelListModel* channels() const { return m_channels; }
    MessageListModel* messages() const { return m_messages; }
    UnreadDmListModel* unreadDirectMessages() const { return m_unreadDms; }
    Preferences* preferences() const { return m_preferences; }
    EmojiPickerModel* emoji() const { return m_emoji; }
    RemoteAuth* qrLogin() const { return m_qrLogin; }

    OfflineCache* offlineCache() const { return m_offline; }

    Q_INVOKABLE void loginWithToken(const QString& token);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void reconnect();

    Q_INVOKABLE void selectDirectMessages();
    Q_INVOKABLE void selectGuild(const QString& guildId);
    Q_INVOKABLE void openChannel(const QString& channelId);
    Q_INVOKABLE void sendMessage(const QString& text, const QString& replyToId = QString());
    // Own messages in the current channel. False when it cannot be sent.
    Q_INVOKABLE bool editMessage(const QString& messageId, const QString& text);
    Q_INVOKABLE void deleteMessage(const QString& messageId);
    // For the info page: name, topic, kind ("dm", "group", "channel"),
    // typeName, category, server, id, nsfw, iconUrl and, for DMs and groups,
    // members [{id, name, username, avatarUrl, blocked}].
    Q_INVOKABLE QVariantMap channelInfo(const QString& channelId) const;
    Q_INVOKABLE void showNotice(const QString& text) { setNotice(text); }
    Q_INVOKABLE void loadOlderMessages();
    Q_INVOKABLE void markCurrentChannelRead();
    Q_INVOKABLE void notifyTyping();
    Q_INVOKABLE void clearNotice();

    // Reactions on messages of the open channel. `emoji` is a Unicode emoji
    // or "name:id" (the `reaction` role of the emoji picker and the `emoji`
    // field of a message's reactions).
    Q_INVOKABLE void addReaction(const QString& messageId, const QString& emoji);
    Q_INVOKABLE void toggleReaction(const QString& messageId, const QString& emoji, bool reacted);
    // Replaces the user's votes on a poll; an empty list removes them.
    Q_INVOKABLE void votePoll(const QString& messageId, const QVariantList& answerIds);
    // Whether GStreamer can play video here (see GstVideoPlayer).
    Q_INVOKABLE bool videoPlaybackAvailable();

    // Called by QtFrontend.
    void coreConnecting();
    void coreConnected();
    void coreLoginAgain();
    void coreLoggedOut();
    void coreSessionClosed(int code);
    void coreGatewayFailed(int gatewayId, const QString& reason);
    void coreGatewayClosed(int gatewayId, int code);
    void coreGatewayMessage(int gatewayId, const std::string& payload);
    void coreSetHeartbeatInterval(int ms);
    void coreMessageAdded(Snowflake channel, const Message& msg);
    void coreMessageUpdated(Snowflake channel, const Message& msg);
    void coreMessageDeleted(Snowflake message);
    void coreFailedToSend(Snowflake channel, Snowflake nonce);
    void coreTyping(Snowflake user, Snowflake guild, Snowflake channel);
    void coreSelectedGuildChanged();
    void coreSelectedChannelChanged();
    void coreChannelListChanged();
    void coreChannelAcknowledged(Snowflake channel);
    void coreGuildListChanged();
    void coreProfileChanged();
    void coreUserChanged(Snowflake user);
    void coreMessagesRefreshed();
    void coreError(const QString& message);

    QString configPath() const;
    bool applicationActive() const;

signals:
    void phaseChanged();
    void connectedChanged();
    void networkOnlineChanged();
    void reconnectSecondsChanged();
    void errorTextChanged();
    void noticeTextChanged();
    void profileChanged();
    void currentGuildChanged();
    void currentChannelChanged();
    void typingTextChanged();
    void loadingMessagesChanged();
    void chatVisibleChanged();
    void autoSelectChannelChanged();

private:
    void setPhase(Phase phase);
    void setConnected(bool connected);
    void setErrorText(const QString& text);
    void setNotice(const QString& text);
    void setLoadingMessages(bool loading);

    void createInstance(const std::string& token);
    void destroyInstance();
    void startGateway();
    void scheduleReconnect();
    void dropGateway();
    void heartbeat();
    // Shows the offline cache while connecting; see OfflineCache.
    void loadCachedState();
    void ensureMessagesLoaded();
    void updateTypingText();
    void afterGuildSelected();
    void refreshUnread();

    Phase m_phase = Starting;
    bool m_connected = false;
    bool m_networkOnline = true;
    int m_reconnectSeconds = 0;
    int m_reconnectDelay = 1;
    QString m_errorText;
    QString m_noticeText;
    QString m_typingText;
    bool m_loadingMessages = false;
    bool m_chatVisible = false;
    bool m_autoSelectChannel = false;
    bool m_firstReadyPending = false;
    QString m_openAfterReady; // channel asked for before the first READY was handled

    QtHttpClient* m_http = nullptr;
    QtWebsocketClient* m_ws = nullptr;
    std::unique_ptr<QtFrontend> m_frontend;
    DiscordInstance* m_instance = nullptr;

    QTimer m_heartbeatTimer;
    bool m_heartbeatAcked = true;
    QTimer m_reconnectTimer;
    QTimer m_reconnectCountdown;
    QTimer m_typingTimer;
    QTimer m_noticeTimer;
    QHash<Snowflake, qint64> m_typingUntil; // user -> ms since epoch
    QSet<Snowflake> m_fetchedChannels;      // history requested since the last READY

    GuildListModel* m_guilds = nullptr;
    ChannelListModel* m_channels = nullptr;
    MessageListModel* m_messages = nullptr;
    UnreadDmListModel* m_unreadDms = nullptr;
    Preferences* m_preferences = nullptr;
    EmojiPickerModel* m_emoji = nullptr;
    OfflineCache* m_offline = nullptr;
    // Showing the offline cache's READY until the real one arrives.
    bool m_cachedStart = false;
    RemoteAuth* m_qrLogin = nullptr;
};
