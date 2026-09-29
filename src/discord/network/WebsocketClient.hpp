#pragma once

// The gateway transport. The core only needs to open, write to and close
// connections, so the host application implements this (Disports uses
// QWebSocket; Discord Messenger wrapped websocketpp/asio here).

#include <string>

namespace CloseCode
{
	enum {
		UNKNOWN_ERROR = 4000,
		UNKNOWN_OPCODE,
		DECODE_ERROR,
		NOT_AUTHENTICATED,
		AUTHENTICATION_FAILED,
		ALREADY_AUTHENTICATED,
		INVALID_SEQ = 4007,
		RATE_LIMITED,
		SESSION_TIMED_OUT,
		INVALID_SHARD,
		SHARDING_REQUIRED,
		INVALID_API_VERSION,
		INVALID_INTENT,
		DISALLOWED_INTENT,
		LOG_ON_AGAIN = 5000,
	};
}

// Standard WebSocket close codes (RFC 6455) used by the core; these replace
// websocketpp::close::status values.
namespace WsCloseStatus
{
	enum {
		NORMAL          = 1000,
		GOING_AWAY      = 1001,
		ABNORMAL_CLOSE  = 1006,
		SERVICE_RESTART = 1012,
	};
}

class WebsocketClient
{
public:
	virtual ~WebsocketClient() {}

	// Opens a connection and returns its ID, or a negative value on failure.
	// Incoming messages, closes and failures are reported through
	// Frontend::OnWebsocketMessage / OnWebsocketClose / OnWebsocketFail.
	virtual int Connect(const std::string& uri) = 0;

	// Closes a connection by ID. Unknown IDs are ignored.
	virtual void Close(int id, int code) = 0;

	// Sends a text message on a connection.
	virtual void SendMsg(int id, const std::string& msg) = 0;
};

// Defined by the host application.
WebsocketClient* GetWebsocketClient();
