# Disports core

The Discord client core of Disports: gateway dispatch, REST requests,
models, caches and settings, in plain C++ without Qt. The Qt app in `src/`
implements the host interfaces and builds the UI on top.

## Origin

This code started as the platform-independent core (`src/core`) of
[Discord Messenger](https://github.com/DiscordMessenger/dm) by
iProgramInCpp, at revision `fca25f5052b241373500dded6ae70a4d83f103d9`
(2026-09-08), used under the MIT license: see
[LICENSE.DiscordMessenger](LICENSE.DiscordMessenger), which must stay with
this code. `protobuf/Protobuf.hpp` is Discord Messenger's homebrew protobuf
reader/writer. `utils/Base64.hpp` is Boost.Beast's base64 helper as modified
by Discord Messenger (Boost Software License 1.0, see its header).

It is now developed as part of Disports and is not kept in sync with
upstream. Useful upstream fixes can still be ported by hand; the changes
below are where the two differ structurally.

## Changes from Discord Messenger

- `network/WebsocketClient.hpp` is an abstract transport (`Connect`,
  `Close`, `SendMsg`) instead of a websocketpp/asio wrapper;
  `WebsocketClient.cpp` is gone. Close codes use `WsCloseStatus`.
- `DiscordInstance::HandleRequest` no longer dumps OpenSSL's error queue.
- Reactions: `Message::m_reactions`, the `MESSAGE_REACTION_*` gateway events
  and `RequestAddReaction` / `RequestRemoveReaction`.
- Embeds carry their video (`RichEmbed::m_bHasVideo`, used for GIFs).
- Embed fields are parsed, and a missing embed timestamp stays 0.
- Sending :name: of an animated server emoji keeps the "a" prefix (<a:name:id>).
- `MessageType` lists every Discord type up to 68; the cache's own row types
  (gaps, pending messages) moved to 10000+, as they collided with 60-69.
- Offline cache support: `Frontend::OnGatewayDispatch` / `OnMessagesFetched`
  hand raw gateway dispatches and fetched history to the front-end,
  `DiscordInstance::LoadCachedReady` replays a saved READY without counting
  as connected (and never resumes its session), and
  `MessageCache::LoadCachedMessages` shows cached messages until the first
  fetch of the channel replaces them (`HasMessages` ignores gap rows).
- Messages carry stickers, call details, the command that produced them and
  role subscription data. Polls: vote events, `RequestPollVote`, and poll
  answers no longer all get id 0.
- Video attachments are recognised (`ContentType::MP4` / `VIDEO`,
  `Attachment::IsVideo`).
- Guild folders carry their colour (`AbstractGuildItem::GetColor`, parsed
  in `SettingsManager::GetGuildFoldersEx`).

## What the host provides

- `GetDiscordInstance()`, `GetFrontend()`, `GetHTTPClient()`,
  `GetWebsocketClient()`.
- A `Frontend` implementation (`Frontend.hpp`).
- The `Md*` text-measuring functions from `text/TextInterface.hpp`, used by
  Discord Messenger's own text layout. A QML UI renders text itself, so
  Disports stubs them.

Disports' implementations are in `src/backend/`.

## Behaviour to know about

- Messages arriving through the gateway are stored in the `MessageCache`
  by the frontend (`OnAddMessage` / `OnUpdateMessage`), not by the core.
- `HandleREADY` opens the first server on the first READY, as a desktop
  client does.
- The core always re-identifies; it does not RESUME and ignores gateway
  RECONNECT (7) / INVALID_SESSION (9) and heartbeat ACKs. Disports' session
  layer handles those for now; they belong in here eventually.
- `RequestMessages` only clears its "in progress" flag for the initial
  load on a channel switch (`HandledChannelSwitch`).
