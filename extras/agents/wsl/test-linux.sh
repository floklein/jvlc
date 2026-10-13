#!/bin/bash
# Run the Qt test suite (model tests + QML tests) of the linux build:
#   JVLC_FILTER=<name-glob> test-linux.sh <key>
# meson rebuilds what changed first. Same settings as the CI debian-meson job.
set -e
. "$(dirname "$0")/env.sh"

KEY="$1"; FILTER="${JVLC_FILTER:-*}"
check_key "$KEY"
B="$JVLC_WORK/$KEY/build-linux"
[ -f "$B/build.ninja" ] || die "no linux build: run jvlc.ps1 build -Platform linux"
lock_tree "$KEY" linux

export VLC_TEST_TIMEOUT=60
names=()
while read -r line; do
    # "qt+test - VLC:test_qt_ml_model"
    n="${line##*:}"
    # shellcheck disable=SC2053
    [[ "$n" == $FILTER ]] && names+=("$n")
done < <(meson test -C "$B" --list --suite qt 2>/dev/null)
[ ${#names[@]} -gt 0 ] || die "no qt test matches '$FILTER'"

meson test -C "$B" --suite qt --print-errorlogs --num-processes "$JVLC_JOBS" "${names[@]}"
