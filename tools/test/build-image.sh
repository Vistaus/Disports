#!/bin/bash
# build-image.sh: (re)builds the test image localhost/disports-test-gl on top
# of clickable's newest amd64 image for this project (made by
# `clickable build --arch amd64`; run that first).
set -e
HERE=$(dirname "$(readlink -f "$0")")
BASE=$(podman images --sort created --format '{{.Repository}}:{{.Tag}}' \
       | grep -E 'clickable/amd64-ut24.04-2.x-amd64-[0-9a-f-]+:' | head -1)
if [ -z "$BASE" ]; then
    echo "no clickable project image yet: run 'clickable build --arch amd64' first" >&2
    exit 1
fi
echo "building on $BASE"
podman build --build-arg BASE="$BASE" -t localhost/disports-test-gl -f "$HERE/Containerfile" "$HERE"
