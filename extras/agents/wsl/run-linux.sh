#!/bin/bash
# Launch the linux build of VLC under WSLg:
#   run-linux.sh <key> <x11|wayland> [vlc args...]
# vlc-static finds the plugins and data of the build tree. The config of each
# worktree is kept in $JVLC_WORK/<key>/home, not in ~/.config/vlc.
set -e
. "$(dirname "$0")/env.sh"

KEY="$1"; MODE="$2"
shift 2 || die "usage: $0 <key> <x11|wayland> [vlc args...]"
check_key "$KEY"
B="$JVLC_WORK/$KEY/build-linux"
[ -x "$B/bin/vlc-static" ] || die "no linux build: run jvlc.ps1 build -Platform linux"

H="$JVLC_WORK/$KEY/home"
export XDG_CONFIG_HOME="$H/config" XDG_DATA_HOME="$H/data" XDG_CACHE_HOME="$H/cache"
export XDG_RUNTIME_DIR="/tmp/jvlc-runtime-$KEY"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
# WSLg sockets (its runtime dir belongs to uid 1000, use absolute paths)
export PULSE_SERVER=unix:/mnt/wslg/PulseServer
# GPU: Mesa's d3d12 driver over WSL's /dev/dxg (else llvmpipe, in software)
if [ -e /dev/dxg ] && [ -d /usr/lib/wsl/lib ]; then
    export LD_LIBRARY_PATH="/usr/lib/wsl/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export GALLIUM_DRIVER=d3d12
fi
case "$MODE" in
    x11)
        export DISPLAY=:0 QT_QPA_PLATFORM=xcb
        unset WAYLAND_DISPLAY
        ;;
    wayland)
        export WAYLAND_DISPLAY=/mnt/wslg/runtime-dir/wayland-0 QT_QPA_PLATFORM=wayland
        unset DISPLAY
        ;;
    *) die "mode must be x11 or wayland" ;;
esac
if [ "$MODE" = wayland ]; then
    exec "$B/bin/vlc-static" -I qt -vv "$@"
fi

# WSLg shows nothing of an X11 window until it is resized (x11-nudge.sh):
# nudge it a few times while the interface loads. jvlc.ps1 nudges it again
# before each screenshot.
"$B/bin/vlc-static" -I qt -vv "$@" &
VLC_PID=$!
trap 'kill $VLC_PID 2>/dev/null' TERM INT
for delay in 1 2 3; do
    sleep $delay
    bash "$(dirname "$0")/x11-nudge.sh" "$KEY" $VLC_PID 2>/dev/null
done
wait $VLC_PID
