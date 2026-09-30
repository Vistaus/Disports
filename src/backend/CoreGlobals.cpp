#include "CoreGlobals.h"

#include "discord/DiscordInstance.hpp"
#include "discord/Frontend.hpp"
#include "discord/network/HTTPClient.hpp"
#include "discord/network/WebsocketClient.hpp"
#include "discord/text/TextInterface.hpp"

#include "QtFrontend.h"
#include "QtHttpClient.h"
#include "QtWebsocketClient.h"

namespace {
DiscordInstance* g_instance = nullptr;
QtFrontend* g_frontend = nullptr;
QtHttpClient* g_http = nullptr;
QtWebsocketClient* g_ws = nullptr;
}

namespace CoreGlobals {
void setInstance(DiscordInstance* instance) { g_instance = instance; }
void setFrontend(QtFrontend* frontend) { g_frontend = frontend; }
void setHttpClient(QtHttpClient* http) { g_http = http; }
void setWebsocketClient(QtWebsocketClient* ws) { g_ws = ws; }
}

DiscordInstance* GetDiscordInstance() { return g_instance; }
Frontend* GetFrontend() { return g_frontend; }
HTTPClient* GetHTTPClient() { return g_http; }
WebsocketClient* GetWebsocketClient() { return g_ws; }

// The core can lay out formatted text itself through these hooks. The QML
// UI renders messages on its own, so they are never meaningfully used.
Point MdMeasureString(DrawingContext*, const String&, int, bool& outWasWordWrapped, int)
{
    outWasWordWrapped = false;
    return Point();
}
int MdLineHeight(DrawingContext*, int) { return 0; }
int MdSpaceWidth(DrawingContext*, int) { return 0; }
void MdDrawString(DrawingContext*, const Rect&, const String&, int) {}
void MdDrawCodeBackground(DrawingContext*, const Rect&) {}
void MdDrawForwardBackground(DrawingContext*, const Rect&) {}
int MdGetQuoteIndentSize() { return 0; }
void MdSetClippingRect(DrawingContext*, const Rect&) {}
void MdClearClippingRect(DrawingContext*) {}
