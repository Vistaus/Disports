#include "QtFrontend.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>

#include "discord/DiscordInstance.hpp"
#include "discord/config/LocalSettings.hpp"
#include "discord/network/DiscordRequest.hpp"
#include "discord/network/HTTPClient.hpp"
#include "discord/state/MessageCache.hpp"

#include "Session.h"

QtFrontend::QtFrontend(Session* session)
    : m_session(session)
{
}

void QtFrontend::OnLoginAgain() { m_session->coreLoginAgain(); }
void QtFrontend::OnLoggedOut() { m_session->coreLoggedOut(); }
void QtFrontend::OnSessionClosed(int errorCode) { m_session->coreSessionClosed(errorCode); }
void QtFrontend::OnConnecting() { m_session->coreConnecting(); }
void QtFrontend::OnConnected() { m_session->coreConnected(); }

void QtFrontend::OnGatewayDispatch(const std::string& type, const nlohmann::json& message)
{
    m_session->offlineCache()->gatewayDispatch(type, message);
    m_session->call()->gatewayDispatch(type, message);
    m_session->voiceStates()->gatewayDispatch(type, message);
}

void QtFrontend::OnMessagesFetched(Snowflake channel, ScrollDir::eScrollDir sd, Snowflake anchor,
                                   const nlohmann::json& messages)
{
    m_session->offlineCache()->messagesFetched(channel, sd, anchor, messages);
}

void QtFrontend::OnAddMessage(Snowflake channelID, const Message& msg)
{
    // The frontend is the one that stores new messages (the core leaves it to it).
    GetMessageCache()->AddMessage(channelID, msg);
    m_session->coreMessageAdded(channelID, msg);
}

void QtFrontend::OnUpdateMessage(Snowflake channelID, const Message& msg)
{
    GetMessageCache()->EditMessage(channelID, msg);
    m_session->coreMessageUpdated(channelID, msg);
}

void QtFrontend::OnDeleteMessage(Snowflake messageInCurrentChannel)
{
    m_session->coreMessageDeleted(messageInCurrentChannel);
}

void QtFrontend::OnStartTyping(Snowflake userID, Snowflake guildID, Snowflake channelID, time_t)
{
    m_session->coreTyping(userID, guildID, channelID);
}

void QtFrontend::OnRequestDone(NetRequest* pRequest)
{
    if (!GetDiscordInstance())
        return;

    // A failed gateway lookup only surfaces as a generic error in the core
    // and is never retried; treat it as a failed connection instead.
    if (pRequest->itype == DiscordRequest::GATEWAY && !pRequest->IsOk()) {
        m_session->coreGatewayFailed(-1, QString::fromStdString(pRequest->ErrorMessage()));
        return;
    }
    GetDiscordInstance()->HandleRequest(pRequest);
}

void QtFrontend::OnFailedToSendMessage(Snowflake channel, Snowflake message)
{
    m_session->coreFailedToSend(channel, message);
}

void QtFrontend::OnFailedToUploadFile(const std::string& file, int error)
{
    m_session->coreUploadFailed(QString::fromStdString(file), error);
}

void QtFrontend::OnStartProgress(Snowflake key, const std::string& fileName, bool isUploading)
{
    if (isUploading)
        m_session->coreUploadStarted(key, QString::fromStdString(fileName));
}

bool QtFrontend::OnUpdateProgress(Snowflake key, size_t offset, size_t length)
{
    return m_session->coreUploadProgress(key, offset, length);
}

void QtFrontend::OnStopProgress(Snowflake key)
{
    m_session->coreUploadStopped(key);
}

void QtFrontend::RefreshMembers(const std::set<Snowflake>&)
{
    m_session->coreMembersChanged();
}

void QtFrontend::OnGenericError(const std::string& message)
{
    m_session->coreError(QString::fromStdString(message));
}

void QtFrontend::OnJsonException(const std::string& message)
{
    qWarning("Discord JSON error: %s", message.c_str());
}

void QtFrontend::OnCantViewChannel(const std::string& channelName)
{
    m_session->coreError(QStringLiteral("You can't view #%1.").arg(QString::fromStdString(channelName)));
}

void QtFrontend::OnGatewayConnectFailure()
{
    m_session->coreGatewayFailed(-1, QStringLiteral("Could not connect to Discord"));
}

void QtFrontend::OnProtobufError(Protobuf::ErrorCode code)
{
    qWarning("Discord settings protobuf error %d", int(code));
}

void QtFrontend::UpdateSelectedGuild() { m_session->coreSelectedGuildChanged(); }
void QtFrontend::UpdateSelectedChannel() { m_session->coreSelectedChannelChanged(); }
void QtFrontend::UpdateChannelList() { m_session->coreChannelListChanged(); }

void QtFrontend::UpdateChannelAcknowledge(Snowflake channelID, Snowflake)
{
    m_session->coreChannelAcknowledged(channelID);
}

void QtFrontend::UpdateProfileAvatar(Snowflake userID, const std::string&) { m_session->coreUserChanged(userID); }
void QtFrontend::UpdateUserData(Snowflake userID) { m_session->coreUserChanged(userID); }
void QtFrontend::RepaintGuildList() { m_session->coreGuildListChanged(); }
void QtFrontend::RepaintProfile() { m_session->coreProfileChanged(); }
void QtFrontend::RepaintProfileWithUserID(Snowflake) { m_session->coreProfileChanged(); }

void QtFrontend::RefreshMessages(ScrollDir::eScrollDir, Snowflake)
{
    m_session->coreMessagesRefreshed();
}

void QtFrontend::LaunchURL(const std::string& url)
{
    QDesktopServices::openUrl(QUrl(QString::fromStdString(url)));
}

void QtFrontend::OnWebsocketMessage(int gatewayID, const std::string& payload)
{
    m_session->coreGatewayMessage(gatewayID, payload);
}

void QtFrontend::OnWebsocketClose(int gatewayID, int errorCode, const std::string&)
{
    m_session->coreGatewayClosed(gatewayID, errorCode);
}

void QtFrontend::OnWebsocketFail(int gatewayID, int, const std::string& message, bool, bool)
{
    m_session->coreGatewayFailed(gatewayID, QString::fromStdString(message));
}

void QtFrontend::SetHeartbeatInterval(int timeMs)
{
    m_session->coreSetHeartbeatInterval(timeMs);
}

std::string QtFrontend::LoadConfig()
{
    QFile file(m_session->configPath());
    if (!file.open(QIODevice::ReadOnly))
        return std::string();
    return file.readAll().toStdString();
}

bool QtFrontend::SaveConfig(const std::string& configJson)
{
    const QString path = m_session->configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    // The config holds the account token: keep it private to the user.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.write(QByteArray::fromStdString(configJson));
    return file.commit();
}

void QtFrontend::RequestQuit()
{
    // Called after errors the core treats as fatal. On a phone the app
    // stays open; the error has already been reported.
}

bool QtFrontend::IsWindowMinimized() { return !m_session->applicationActive(); }
bool QtFrontend::IsWindowFocused() { return m_session->applicationActive(); }

std::string QtFrontend::GetDirectMessagesText() { return "Direct Messages"; }
std::string QtFrontend::GetPleaseWaitText() { return "Please wait..."; }

std::string QtFrontend::GetMonthName(int index)
{
    static const char* const months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December",
    };
    if (index < 0 || index >= 12)
        return std::string();
    return months[index];
}

std::string QtFrontend::GetTodayAtText() { return "Today at "; }
std::string QtFrontend::GetYesterdayAtText() { return "Yesterday at "; }
std::string QtFrontend::GetFormatDateOnlyText() { return "%s %d%s, %d"; }
std::string QtFrontend::GetFormatTimeLongText() { return "%d-%m-%Y at " + GetFormatTimestampTimeShort(); }
std::string QtFrontend::GetFormatTimeShortText() { return "%d/%m " + GetFormatTimestampTimeShort(); }
std::string QtFrontend::GetFormatTimeShorterText() { return GetFormatTimestampTimeShort(); }

std::string QtFrontend::GetFormatTimestampTimeShort()
{
    return GetLocalSettings()->Use12HourTime() ? "%I:%M %p" : "%H:%M";
}

std::string QtFrontend::GetFormatTimestampTimeLong() { return "%H:%M:%S"; }
std::string QtFrontend::GetFormatTimestampDateShort() { return "%d/%m/%Y"; }
std::string QtFrontend::GetFormatTimestampDateLong() { return "%e %B %Y"; }

std::string QtFrontend::GetFormatTimestampDateLongTimeShort()
{
    return GetFormatTimestampDateLong() + " " + GetFormatTimestampTimeShort();
}

std::string QtFrontend::GetFormatTimestampDateLongTimeLong()
{
    return "%A, " + GetFormatTimestampDateLong() + " " + GetFormatTimestampTimeShort();
}

#ifdef USE_DEBUG_PRINTS
void QtFrontend::DebugPrint(const char* fmt, va_list vl)
{
    char buffer[2048];
    vsnprintf(buffer, sizeof buffer, fmt, vl);
    qDebug("%s", buffer);
}
#endif
