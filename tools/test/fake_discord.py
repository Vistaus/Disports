#!/usr/bin/env python3
"""A small fake Discord (REST + gateway) for testing Disports on the PC.

REST:    http://127.0.0.1:8811/api/v9/   (token: test-token)
Gateway: ws://127.0.0.1:8812/

One server, "Test Server", where our user (100) has the role "Muted":
  #general        normal
  #muted-hidden   the Muted role is denied View Channel (the role-deny bug)
  #read-only      @everyone is denied Send Messages
  #no-history     @everyone is denied Read Message History
  #slow           slowmode of 10 s
  #no-files       @everyone is denied Attach Files
Roles: Moderators (mentionable), Muted. Members: alice, bob, carol, dave.
A DM with Alice (2001).

Uploads follow Discord's two steps: POST .../attachments gives an upload
URL, PUT sends the file there, then the message names the upload.
Everything received is logged to stdout, one line per event.
"""
import asyncio
import json
import os
import re
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import websockets

TOKEN = "test-token"
BASE = "http://127.0.0.1:8811"


def user(uid, name, glob):
    return {"id": uid, "username": name, "global_name": glob, "avatar": None, "discriminator": "0"}


ME = user("100", "tester", "Test User")
ALICE = user("200", "alice", "Alice")
BOB = user("300", "bob", "Bob")
CAROL = user("400", "carol", "Carol")
DAVE = user("500", "dave", "Dave")
MEMBERS = [ALICE, BOB, CAROL, DAVE]
# More people, for a full voice channel.
EXTRA = [user(str(600 + i), "guest%d" % i, "Guest %d" % i) for i in range(4)]

GUILD = "1000"
ROLE_MODS = "1900"
ROLE_MUTED = "1901"
VIEW, SEND, HISTORY, ATTACH, MENTION_EVERYONE = 0x400, 0x800, 0x10000, 0x8000, 0x20000
EVERYONE_PERMS = 0x400 | 0x800 | 0x10000 | 0x8000 | 0x40 | 0x100000 | 0x200000

CATEGORY, GENERAL, HIDDEN, READONLY, NOHISTORY, SLOW, NOFILES = "1100", "1101", "1102", "1103", "1104", "1105", "1106"
LOUNGE, EMPTY_VOICE = "1107", "1108"
QUIET, QUIET_CHAN = "5000", "5001"
DM = "2001"


def overwrite(target, allow=0, deny=0, member=False):
    return {"id": target, "type": 1 if member else 0, "allow": str(allow), "deny": str(deny)}


CHANNELS = [
    {"id": CATEGORY, "type": 4, "name": "Text Channels", "position": 0},
    {"id": GENERAL, "type": 0, "name": "general", "position": 0, "parent_id": CATEGORY},
    {"id": HIDDEN, "type": 0, "name": "muted-hidden", "position": 1, "parent_id": CATEGORY,
     "permission_overwrites": [overwrite(ROLE_MUTED, deny=VIEW)]},
    {"id": READONLY, "type": 0, "name": "read-only", "position": 2, "parent_id": CATEGORY,
     "permission_overwrites": [overwrite(GUILD, deny=SEND)]},
    {"id": NOHISTORY, "type": 0, "name": "no-history", "position": 3, "parent_id": CATEGORY,
     "permission_overwrites": [overwrite(GUILD, deny=HISTORY)]},
    {"id": SLOW, "type": 0, "name": "slow", "position": 4, "parent_id": CATEGORY, "rate_limit_per_user": 10},
    {"id": NOFILES, "type": 0, "name": "no-files", "position": 5, "parent_id": CATEGORY,
     "permission_overwrites": [overwrite(GUILD, deny=ATTACH)]},
    {"id": LOUNGE, "type": 2, "name": "Lounge", "position": 6, "parent_id": CATEGORY},
    {"id": EMPTY_VOICE, "type": 2, "name": "Quiet room", "position": 7, "parent_id": CATEGORY},
]
# Seven people in the Lounge: five are listed, then "and 2 more".
LOUNGE_PEOPLE = [ALICE, BOB, CAROL, DAVE] + EXTRA[:3]


def snowflake_at(ms):
    return str((int(ms) - 1420070400000) << 22)


# A DM call with Alice, started three minutes ago.
DM_CALL_MESSAGE = snowflake_at(time.time() * 1000 - 180000)

HISTORY_MSGS = {}
UPLOADS = {}
seq = 0
clients = set()
loop = None
next_id = 3000


def log(*args):
    print(time.strftime("%H:%M:%S"), *args, flush=True)


def new_id():
    global next_id
    next_id += 1
    return str(next_id)


def iso(ts):
    return time.strftime("%Y-%m-%dT%H:%M:%S.000000+00:00", time.gmtime(ts))


def message(channel, author, content, **extra):
    m = {"id": new_id(), "channel_id": channel, "author": author, "content": content,
         "timestamp": iso(time.time()), "edited_timestamp": None, "tts": False,
         "mention_everyone": False, "mentions": [], "mention_roles": [], "attachments": [],
         "embeds": [], "pinned": False, "type": 0}
    m.update(extra)
    return m


for ch in (GENERAL, NOHISTORY, SLOW, NOFILES, READONLY, DM):
    HISTORY_MSGS[ch] = [message(ch, ALICE, "Hello from Alice in %s" % ch)]


def ready():
    for ch in CHANNELS:
        if ch["id"] in HISTORY_MSGS:
            ch["last_message_id"] = HISTORY_MSGS[ch["id"]][-1]["id"]
    roles = [
        {"id": GUILD, "name": "@everyone", "permissions": str(EVERYONE_PERMS), "position": 0,
         "color": 0, "hoist": False, "managed": False, "mentionable": False},
        {"id": ROLE_MODS, "name": "Moderators", "permissions": "0", "position": 2,
         "color": 0x3498db, "hoist": True, "managed": False, "mentionable": True},
        {"id": ROLE_MUTED, "name": "Muted", "permissions": "0", "position": 1,
         "color": 0, "hoist": False, "managed": False, "mentionable": False},
    ]
    return {
        "v": 9, "session_id": "s1", "session_type": "normal",
        "resume_gateway_url": "ws://127.0.0.1:8812",
        "user": ME,
        "user_settings_proto": "",
        "guilds": [{"id": GUILD, "properties": {"name": "Test Server", "icon": None, "owner_id": "300"},
                    "channels": CHANNELS, "roles": roles, "emojis": [], "member_count": 9,
                    "voice_states": [{"user_id": u["id"], "channel_id": LOUNGE, "session_id": "x",
                                      "self_mute": False, "self_deaf": False} for u in LOUNGE_PEOPLE]},
                   {"id": QUIET, "properties": {"name": "Quiet Server", "icon": None, "owner_id": "300"},
                    "channels": [{"id": QUIET_CHAN, "type": 0, "name": "chat", "position": 0}],
                    "roles": [dict(roles[0], id=QUIET)], "emojis": [], "voice_states": []}],
        "users": MEMBERS,
        "merged_members": [[{"user_id": "100", "roles": [ROLE_MUTED], "nick": None}],
                           [{"user_id": "100", "roles": [], "nick": None}]],
        "private_channels": [{"id": DM, "type": 1, "recipient_ids": ["200"],
                              "last_message_id": HISTORY_MSGS[DM][-1]["id"]}],
        "read_state": {"entries": [], "version": 1},
        "relationships": [{"id": "200", "user_id": "200", "type": 1, "user": ALICE}],
        "user_guild_settings": {"entries": [], "version": 0},
        "sessions": [], "guild_join_requests": [], "connected_accounts": [],
    }


async def dispatch(ws, event, data):
    global seq
    seq += 1
    await ws.send(json.dumps({"op": 0, "t": event, "s": seq, "d": data}))


def broadcast(event, data):
    for ws in list(clients):
        asyncio.run_coroutine_threadsafe(dispatch(ws, event, data), loop)


async def gateway(ws):
    log("GATEWAY connect")
    await ws.send(json.dumps({"op": 10, "d": {"heartbeat_interval": 41250}}))
    try:
        async for raw in ws:
            msg = json.loads(raw)
            op, d = msg.get("op"), msg.get("d")
            if op == 1:
                await ws.send(json.dumps({"op": 11}))
            elif op == 2:
                if d.get("token") != TOKEN:
                    await ws.close(4004, "Authentication failed")
                    return
                clients.add(ws)
                await dispatch(ws, "READY", ready())
                await dispatch(ws, "READY_SUPPLEMENTAL", {"guilds": [{"id": GUILD}, {"id": QUIET}],
                               "merged_presences": {"friends": [], "guilds": [[], []]}})
                # Calls going on in DMs arrive after READY.
                await dispatch(ws, "CALL_CREATE", {"channel_id": DM, "message_id": DM_CALL_MESSAGE,
                               "region": "x", "ringing": [], "voice_states": [
                                   {"user_id": "200", "channel_id": DM, "session_id": "y"}]})
            elif op == 8:
                query = (d.get("query") or "").lower()
                ids = d.get("user_ids") or []
                everyone = MEMBERS + EXTRA
                if ids:
                    found = [m for m in everyone if m["id"] in [str(i) for i in ids]]
                else:
                    found = [m for m in MEMBERS if m["username"].startswith(query) or m["global_name"].lower().startswith(query)]
                log("GATEWAY member search", json.dumps(query or ids), "->", [m["username"] for m in found])
                await dispatch(ws, "GUILD_MEMBERS_CHUNK", {
                    "guild_id": d.get("guild_id") if isinstance(d.get("guild_id"), str) else GUILD,
                    "members": [{"user": m, "roles": [ROLE_MODS] if m is BOB else [], "nick": None,
                                 "joined_at": iso(0)} for m in found],
                    "not_found": [], "chunk_index": 0, "chunk_count": 1})
            else:
                log("GATEWAY op", op, json.dumps(d)[:200])
    except websockets.ConnectionClosed:
        pass
    finally:
        clients.discard(ws)


class Rest(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, status, body=None, raw=None, content_type="application/json"):
        data = raw if raw is not None else (json.dumps(body).encode() if body is not None else b"")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def body(self):
        length = int(self.headers.get("Content-Length") or 0)
        return self.rfile.read(length)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/api/v9/gateway":
            return self.reply(200, {"url": "ws://127.0.0.1:8812"})
        m = re.match(r"/api/v9/channels/(\d+)/messages$", path)
        if m:
            channel = m.group(1)
            log("REST history", channel)
            return self.reply(200, list(reversed(HISTORY_MSGS.get(channel, []))))
        m = re.match(r"/files/(\d+)/(.+)$", path)
        if m and m.group(1) in UPLOADS:
            return self.reply(200, raw=UPLOADS[m.group(1)]["data"], content_type="application/octet-stream")
        return self.reply(200, {})

    def do_PUT(self):
        m = re.match(r"/upload/(\d+)$", self.path)
        if m and m.group(1) in UPLOADS:
            data = self.body()
            UPLOADS[m.group(1)]["data"] = data
            log("REST upload PUT", m.group(1), len(data), "bytes, content-type",
                self.headers.get("Content-Type"))
            return self.reply(200)
        return self.reply(404, {"message": "Unknown"})

    def do_POST(self):
        raw = self.body()
        try:
            body = json.loads(raw or b"{}")
        except ValueError:
            body = {}
        path = self.path
        m = re.match(r"/api/v9/channels/(\d+)/attachments$", path)
        if m:
            results = []
            for f in body.get("files", []):
                upload = new_id()
                UPLOADS[upload] = {"name": f["filename"], "size": f.get("file_size"), "data": b""}
                results.append({"id": int(f["id"]), "upload_url": "%s/upload/%s" % (BASE, upload),
                                "upload_filename": "uploads/%s/%s" % (upload, f["filename"])})
            log("REST attachments", json.dumps(body))
            return self.reply(200, {"attachments": results})
        m = re.match(r"/api/v9/channels/(\d+)/messages$", path)
        if m:
            channel = m.group(1)
            log("REST send", channel, json.dumps(body, ensure_ascii=False))
            attachments = []
            for a in body.get("attachments", []):
                upload = a["uploaded_filename"].split("/")[1]
                info = UPLOADS.get(upload, {})
                attachments.append({"id": new_id(), "filename": a["filename"], "size": len(info.get("data", b"")),
                                    "url": "%s/files/%s/%s" % (BASE, upload, a["filename"]),
                                    "proxy_url": "%s/files/%s/%s" % (BASE, upload, a["filename"]),
                                    "content_type": "image/png" if a["filename"].endswith(".png") else "text/plain"})
            msg = message(channel, ME, body.get("content", ""), nonce=body.get("nonce"), attachments=attachments)
            if channel != DM:
                msg["guild_id"] = GUILD
            HISTORY_MSGS.setdefault(channel, []).append(msg)
            broadcast("MESSAGE_CREATE", msg)
            return self.reply(200, msg)
        if path.endswith("/ack"):
            return self.reply(200, {"token": None})
        if path.endswith("/typing"):
            return self.reply(204)
        log("REST POST (unhandled)", path)
        return self.reply(404, {"message": "Unknown"})


async def main():
    global loop
    loop = asyncio.get_running_loop()
    server = ThreadingHTTPServer(("127.0.0.1", 8811), Rest)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    async with websockets.serve(gateway, "127.0.0.1", 8812):
        log("fake discord ready")
        await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())
