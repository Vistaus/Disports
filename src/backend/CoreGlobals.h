#pragma once

// The global accessors Discord Messenger's core expects the host to define
// (GetDiscordInstance, GetFrontend, GetHTTPClient, GetWebsocketClient) are
// backed by these pointers, which the Session sets up and tears down.

class DiscordInstance;
class QtFrontend;
class QtHttpClient;
class QtWebsocketClient;

namespace CoreGlobals {
void setInstance(DiscordInstance* instance);
void setFrontend(QtFrontend* frontend);
void setHttpClient(QtHttpClient* http);
void setWebsocketClient(QtWebsocketClient* ws);
}
