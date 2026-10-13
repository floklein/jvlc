#!/bin/bash
# WSLg quirk: its XWayland is older than 21.1 (so VLC also falls back from its
# X11 compositor to the dummy one) and does not show what an X11 window draws
# until the window is resized. Resize VLC's window by one pixel and back.
#   x11-nudge.sh <key> [vlc pid]
. "$(dirname "$0")/env.sh"

KEY="$1"; PID="$2"
check_key "$KEY"
export DISPLAY=:0
if [ -z "$PID" ]; then
    PID=$(pgrep -f -n "^$JVLC_WORK/$KEY/build-linux/bin/vlc-static") || exit 0
fi
WIN=$(xdotool search --onlyvisible --pid "$PID" 2>/dev/null | head -n1)
[ -n "$WIN" ] || exit 1
eval "$(xdotool getwindowgeometry --shell "$WIN")"
xdotool windowsize "$WIN" $((WIDTH + 1)) "$HEIGHT"
sleep 0.3
xdotool windowsize "$WIN" "$WIDTH" "$HEIGHT"
