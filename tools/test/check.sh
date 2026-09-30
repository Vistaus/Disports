#!/bin/bash
# check.sh [--no-build] [scenario ...]
#
# Builds the app with AddressSanitizer, LeakSanitizer and UBSan
# (-DDISPORTS_SANITIZE=ON, in build/asan/), then runs each scenario against
# the fake Discord server and fails when
#   - memory is misused or behaviour is undefined in any run,
#   - memory allocated by our code (src/) is still unfreed at exit
#     (libraries' own leftovers are listed but do not fail; leaks.py),
#   - the app does not exit cleanly, or
#   - the server did not receive what the scenario should send.
# Screenshots and logs: build/test/<scenario>/.
#
# Needs podman, clickable and the test image (build-image.sh). See README.md.
set -u
HERE=$(dirname "$(readlink -f "$0")")
ROOT=$(readlink -f "$HERE/../..")
LOG=$ROOT/build/test/fake.log
export APP_INSTALL=$ROOT/build/asan/app/install

BUILD=1
if [ "${1:-}" = "--no-build" ]; then BUILD=0; shift; fi

podman image exists localhost/disports-test-gl \
    || { echo "no test image: run tools/test/build-image.sh"; exit 1; }

if [ $BUILD = 1 ]; then
    # clickable.yaml plus the sanitizer option and its own build folder.
    CONFIG=$ROOT/.clickable-asan.yaml
    sed -e 's|^  - -DCLICK_MODE=ON|  - -DCLICK_MODE=ON\n  - -DDISPORTS_SANITIZE=ON|' "$ROOT/clickable.yaml" > "$CONFIG"
    echo 'build_dir: ${ROOT}/build/asan/app' >> "$CONFIG"
    echo "== building the sanitizer build"
    (cd "$ROOT" && clickable build --arch amd64 -c "$CONFIG") > "$ROOT/build/asan-build.log" 2>&1 \
        || { echo "build failed, see build/asan-build.log"; tail -20 "$ROOT/build/asan-build.log"; exit 1; }
fi

WIDE=(DISPORTS_WIDTH=1000 DISPORTS_HEIGHT=800)
FAILED=0

# expect <scenario> <extended regex>: the server log must have a match.
expect() {
    if ! grep -a -q -E "$2" "$LOG"; then
        echo "   FAIL $1: the server never received /$2/"
        FAILED=1
    fi
}

scenario_channels() {   # a server's channels and a text channel's history
    STEPS="sleep 5" "$HERE/run.sh" channels 7000 DISPORTS_OPEN_CHANNEL=1101 "${WIDE[@]}"
    expect channels 'REST history 1101'
}
scenario_mentions() {   # pick @bob and #slow from the suggestions, send
    STEPS="sleep 5;click 700 772;sleep 1;type hi @bo;sleep 2;click 600 715;sleep 1;type see #sl;sleep 1;click 600 715;sleep 1;key Return;sleep 2" \
        "$HERE/run.sh" mentions 16000 DISPORTS_OPEN_CHANNEL=1101 "${WIDE[@]}"
    expect mentions 'member search "bo"'
    expect mentions 'REST send 1101 .*"hi <@300> see <#1105>"'
}
scenario_upload() {     # a file with a message
    mkdir -p "$ROOT/build/test/files"
    python3 - "$ROOT/build/test/files/upload.png" <<'PY'
import struct, sys, zlib
w, h = 64, 48
raw = b"".join(b"\0" + bytes([200, 60, 120]) * w for _ in range(h))
chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
open(sys.argv[1], "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                              + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
PY
    STEPS="sleep 7" "$HERE/run.sh" upload 9000 DISPORTS_OPEN_CHANNEL=1101 "${WIDE[@]}" \
        DISPORTS_SEND_FILE=/files/upload.png "DISPORTS_SEND_FILE_TEXT=a picture"
    expect upload 'REST upload PUT [0-9]+ 165 bytes'
    expect upload 'REST send 1101 .*"filename": "upload.png"'
}
scenario_dmcall() {     # a DM with a call going on
    STEPS="sleep 5" "$HERE/run.sh" dmcall 7000 DISPORTS_OPEN_CHANNEL=2001 "${WIDE[@]}"
    expect dmcall 'REST history 2001'
}
scenario_permissions() { # read-only, no history, no files, slowmode
    for channel in 1103 1104 1106 1105; do
        STEPS="sleep 5" "$HERE/run.sh" permissions-$channel 6500 DISPORTS_OPEN_CHANNEL=$channel "${WIDE[@]}"
    done
}

ALL=(channels mentions upload dmcall permissions)
SCENARIOS=("${@:-${ALL[@]}}")
[ $# -eq 0 ] && SCENARIOS=("${ALL[@]}")

RUNS=()
for name in "${SCENARIOS[@]}"; do
    echo "== $name"
    "$HERE/server.sh" restart >/dev/null
    "scenario_$name"
    for out in "$ROOT"/build/test/$name*; do
        RUNS+=("$out")
        grep -q "app exit: 0" "$out/log.txt" || { echo "   FAIL $(basename "$out"): the app did not exit cleanly"; FAILED=1; }
    done
done

# Leaks at exit are expected from Qt, Mesa, FreeType and the toolkit;
# only ours fail (see leaks.py), with memory errors and undefined behaviour.
echo "== sanitizer reports"
reports=()
for out in "${RUNS[@]}"; do
    reports+=($(ls "$out"/asan.* "$out"/ubsan.* 2>/dev/null))
done
if [ ${#reports[@]} -gt 0 ]; then
    summary=$("$HERE/leaks.py" "${reports[@]}") || FAILED=1
    echo "$summary" | cut -c1-200
else
    echo "   none"
fi
"$HERE/server.sh" stop

[ $FAILED = 0 ] && echo "== all good" || echo "== FAILED"
exit $FAILED
