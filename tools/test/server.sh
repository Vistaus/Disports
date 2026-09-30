#!/bin/bash
# server.sh start|stop|restart: the fake Discord server (fake_discord.py),
# logging to build/test/fake.log. Its Python environment lives in
# build/test/venv (made on first use).
set -e
HERE=$(dirname "$(readlink -f "$0")")
ROOT=$(readlink -f "$HERE/../..")
OUT=$ROOT/build/test
mkdir -p "$OUT"
if [ ! -x "$OUT/venv/bin/python" ]; then
    python3 -m venv "$OUT/venv"
    "$OUT/venv/bin/pip" install -q -r "$HERE/requirements.txt"
fi
stop() {
    if [ -f "$OUT/fake.pid" ]; then
        kill "$(cat "$OUT/fake.pid")" 2>/dev/null || true
        rm -f "$OUT/fake.pid"
        sleep 0.3
    fi
}
start() {
    if ss -ltn 2>/dev/null | grep -q -E ':(8811|8812) '; then
        echo "ports 8811/8812 are taken (another fake server?):" >&2
        ss -ltnp 2>/dev/null | grep -E ':(8811|8812) ' >&2
        exit 1
    fi
    # A fresh log each time (not truncated under the running server).
    rm -f "$OUT/fake.log"
    nohup "$OUT/venv/bin/python" "$HERE/fake_discord.py" > "$OUT/fake.log" 2>&1 &
    echo $! > "$OUT/fake.pid"
    for _ in $(seq 20); do grep -q "fake discord ready" "$OUT/fake.log" 2>/dev/null && return; sleep 0.2; done
    echo "fake server did not start:" >&2; cat "$OUT/fake.log" >&2; exit 1
}
case $1 in
    start) stop; start ;;
    stop) stop ;;
    restart) stop; start ;;
    *) echo "usage: $0 start|stop|restart" >&2; exit 2 ;;
esac
