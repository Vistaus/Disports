#pragma once

#include <QObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <nlohmann/json.hpp>

#include "discord/models/Snowflake.hpp"

class QThread;
class Session;
class VoiceSession;
class Ringtone;

// Voice calls: DM and group calls (with ringing) and voice channels.
//
// Joining sends a voice state update on the main gateway, which answers
// with a session id and a voice server; VoiceSession connects to that on
// its own thread.
class CallManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString channelId READ channelId NOTIFY stateChanged)
    Q_PROPERTY(bool directCall READ directCall NOTIFY stateChanged)
    Q_PROPERTY(QString title READ title NOTIFY stateChanged)
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
    Q_PROPERTY(bool deafened READ deafened NOTIFY mutedChanged)
    // False in voice channels without the "Speak" permission: muted.
    Q_PROPERTY(bool canSpeak READ canSpeak NOTIFY stateChanged)
    Q_PROPERTY(bool speaker READ speaker NOTIFY speakerChanged)
    Q_PROPERTY(bool speakerAvailable READ speakerAvailable NOTIFY speakerChanged)
    // [{id, name, avatarUrl, speaking, muted, deafened, self, joined}], us
    // first.
    // In DM calls, people still being rung have joined: false.
    Q_PROPERTY(QVariantList participants READ participants NOTIFY participantsChanged)

public:
    enum State {
        Idle,
        Incoming,   // a DM call is ringing us
        Connecting, // joined, setting up the voice connection
        Active,     // connected (a DM call may still be ringing the others)
    };
    Q_ENUM(State)

    explicit CallManager(Session* session);
    ~CallManager() override;

    State state() const { return m_state; }
    QString channelId() const;
    bool directCall() const { return m_direct; }
    QString title() const { return m_title; }
    QString avatarUrl() const { return m_avatarUrl; }
    QString statusText() const;
    bool muted() const { return m_muted; }
    bool deafened() const { return m_deafened; }
    bool canSpeak() const { return m_canSpeak; }
    bool speaker() const { return m_speaker; }
    bool speakerAvailable() const { return m_speakerAvailable; }
    QVariantList participants() const;

    // Calls a DM or group, or joins a voice channel.
    Q_INVOKABLE void start(const QString& channelId);
    Q_INVOKABLE void accept();
    Q_INVOKABLE void decline();
    Q_INVOKABLE void hangUp();
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void toggleDeafen();
    Q_INVOKABLE void toggleSpeaker();
    Q_INVOKABLE bool canCall(const QString& channelId) const;
    // Brings the call screen back.
    Q_INVOKABLE void show();

    // From the main gateway (see QtFrontend::OnGatewayDispatch).
    void gatewayDispatch(const std::string& type, const nlohmann::json& message);
    // The main gateway went away: the call cannot go on.
    void gatewayLost();

signals:
    void stateChanged();
    void statusTextChanged();
    void mutedChanged();
    void speakerChanged();
    void participantsChanged();
    void callFailed(const QString& reason);
    void notice(const QString& text);
    // Started, ringing, or asked for.
    void showRequested();

private:
    void setState(State state);
    void describe(Snowflake channel);
    void join(Snowflake channel);
    void maybeConnect();
    void endCall(const QString& reason = QString());
    void stopVoice();
    void setVoiceConnected(bool connected);
    void addParticipant(Snowflake user);
    void removeParticipant(Snowflake user);
    void keepDisplayOn(bool on);
    void updateRingtone();

    Session* m_session;
    State m_state = Idle;
    Snowflake m_channel = 0;
    Snowflake m_guild = 0;
    bool m_direct = false;
    QString m_title;
    QString m_avatarUrl;
    bool m_muted = false;
    bool m_deafened = false;
    bool m_canSpeak = true;
    bool m_mutedByPermission = false; // unmuted again after the call
    bool m_speaker = true; // calls start on the loudspeaker
    bool m_speakerAvailable = false;

    // From the gateway, for the voice server.
    QString m_voiceSessionId;
    QString m_voiceToken;
    QString m_voiceEndpoint;
    bool m_voiceConnected = false;

    QList<Snowflake> m_participants; // in the order they joined
    QSet<Snowflake> m_speaking;
    qint64 m_talkStartedMs = 0; // someone else joined / the channel was joined
    QTimer m_clock;

    QThread* m_thread = nullptr;
    VoiceSession* m_voice = nullptr;
    Ringtone* m_ringtone = nullptr;
    uint m_displayCookie = 0;
    bool m_displayRequested = false;
};
