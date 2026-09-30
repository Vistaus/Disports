#pragma once

#include "discord/Frontend.hpp"

class Session;

// Discord Messenger's Frontend interface, implemented for Disports. Every
// call arrives on the Qt main thread (both transports deliver there) and is
// forwarded to the Session, which owns the QML-facing state.
class QtFrontend : public Frontend
{
public:
    explicit QtFrontend(Session* session);

    // Events
    void OnLoginAgain() override;
    void OnLoggedOut() override;
    void OnSessionClosed(int errorCode) override;
    void OnConnecting() override;
    void OnConnected() override;
    void OnGatewayDispatch(const std::string& type, const nlohmann::json& message) override;
    void OnMessagesFetched(Snowflake channel, ScrollDir::eScrollDir sd, Snowflake anchor,
                           const nlohmann::json& messages) override;
    void OnAddMessage(Snowflake channelID, const Message& msg) override;
    void OnUpdateMessage(Snowflake channelID, const Message& msg) override;
    void OnDeleteMessage(Snowflake messageInCurrentChannel) override;
    void OnStartTyping(Snowflake userID, Snowflake guildID, Snowflake channelID, time_t startTime) override;
    void OnAttachmentDownloaded(bool, const uint8_t*, size_t, const std::string&) override {}
    void OnAttachmentFailed(bool, const std::string&) override {}
    void OnRequestDone(NetRequest* pRequest) override;
    void OnLoadedPins(Snowflake, const std::string&) override {}
    void OnUpdateAvailable(const std::string&, const std::string&) override {}
    void OnFailedToSendMessage(Snowflake channel, Snowflake message) override;
    void OnFailedToUploadFile(const std::string& file, int error) override;
    void OnFailedToCheckForUpdates(int, const std::string&) override {}
    void OnStartProgress(Snowflake, const std::string&, bool) override {}
    bool OnUpdateProgress(Snowflake, size_t, size_t) override { return true; }
    void OnStopProgress(Snowflake) override {}
    void OnNotification() override {}

    // Error messages
    void OnGenericError(const std::string& message) override;
    void OnJsonException(const std::string& message) override;
    void OnCantViewChannel(const std::string& channelName) override;
    void OnGatewayConnectFailure() override;
    void OnProtobufError(Protobuf::ErrorCode code) override;

    // Update requests
    void UpdateSelectedGuild() override;
    void UpdateSelectedChannel() override;
    void UpdateChannelList() override;
    void UpdateMemberList() override {}
    void UpdateChannelAcknowledge(Snowflake channelID, Snowflake messageID) override;
    void UpdateProfileAvatar(Snowflake userID, const std::string& resid) override;
    void UpdateProfilePopout(Snowflake) override {}
    void UpdateUserData(Snowflake userID) override;
    void UpdateAttachment(Snowflake) override {}
    void RepaintGuildList() override;
    void RepaintProfile() override;
    void RepaintProfileWithUserID(Snowflake id) override;
    void RefreshMessages(ScrollDir::eScrollDir sd, Snowflake gapCulprit) override;
    void RefreshMembers(const std::set<Snowflake>&) override {}

    // Interactive requests
    void JumpToMessage(Snowflake) override {}
    void LaunchURL(const std::string& url) override;

    // Websocket
    void OnWebsocketMessage(int gatewayID, const std::string& payload) override;
    void OnWebsocketClose(int gatewayID, int errorCode, const std::string& message) override;
    void OnWebsocketFail(int gatewayID, int errorCode, const std::string& message, bool isTLSError, bool mayRetry) override;

    void SetHeartbeatInterval(int timeMs) override;

    // Images are loaded by QML straight from the CDN.
    void RegisterIcon(Snowflake, const std::string&) override {}
    void RegisterAvatar(Snowflake, const std::string&) override {}
    void RegisterAttachment(Snowflake, const std::string&) override {}
    void RegisterChannelIcon(Snowflake, const std::string&) override {}

    // Config
    std::string LoadConfig() override;
    bool SaveConfig(const std::string& configJson) override;

    void RequestQuit() override;

    bool IsWindowMinimized() override;
    bool IsWindowFocused() override;

    // Strings
    std::string GetDirectMessagesText() override;
    std::string GetPleaseWaitText() override;
    std::string GetMonthName(int index) override;
    std::string GetTodayAtText() override;
    std::string GetYesterdayAtText() override;
    std::string GetFormatDateOnlyText() override;
    std::string GetFormatTimeLongText() override;
    std::string GetFormatTimeShortText() override;
    std::string GetFormatTimeShorterText() override;
    std::string GetFormatTimestampTimeShort() override;
    std::string GetFormatTimestampTimeLong() override;
    std::string GetFormatTimestampDateShort() override;
    std::string GetFormatTimestampDateLong() override;
    std::string GetFormatTimestampDateLongTimeShort() override;
    std::string GetFormatTimestampDateLongTimeLong() override;

    // Window management does not apply on a phone.
    void HideWindow() override {}
    void RestoreWindow() override {}
    void MaximizeWindow() override {}
    int GetMinimumWidth() override { return 0; }
    int GetMinimumHeight() override { return 0; }
    int GetDefaultWidth() override { return 0; }
    int GetDefaultHeight() override { return 0; }

#ifdef USE_DEBUG_PRINTS
    void DebugPrint(const char* fmt, va_list vl) override;
#endif

    bool UseGradientByDefault() override { return false; }

private:
    Session* m_session;
};
