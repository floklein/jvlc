#!/bin/sh
# Root part of the one-time setup of a jvlc build distro. Run by
# `jvlc.ps1 setup` as `wsl -d <distro> -u root -- sh setup-root.sh <win64|linux>`.
# Idempotent.
set -e

# Default user, interop on, no Windows PATH (it would leak Windows tools).
cat > /etc/wsl.conf <<EOF
[user]
default=videolan
[interop]
enabled=true
appendWindowsPath=false
[boot]
systemd=false
EOF

mkdir -p /opt/jvlc
chown videolan:videolan /opt/jvlc

case "$1" in
    linux)
        # The image is Debian sid frozen on its build date. Install from the
        # matching snapshot so apt never upgrades the image's Qt or libc.
        cat > /etc/apt/sources.list.d/debian.sources <<EOF
Types: deb
URIs: http://snapshot.debian.org/archive/debian/20260611T000000Z
Suites: sid
Components: main
Signed-By: /usr/share/keyrings/debian-archive-keyring.pgp
Check-Valid-Until: no
EOF
        rm -f /etc/apt/sources.list
        # Runtime pieces the CI image lacks to *show* VLC under WSLg:
        # qt6-wayland (Wayland QPA plugin), qt6-svg-plugins (VLC's SVG icons),
        # xdotool (run-linux.sh, X11 first-frame workaround)
        PKGS="ccache qt6-wayland qt6-svg-plugins xdotool"
        ;;
    win64)
        # The prebuilt contribs keep a few absolute paths of the CI checkout
        # (/builds/videolan/vlc/contrib/...: Qt's chainloaded toolchain file,
        # some CMake configs). Make them resolve to the shared contribs.
        mkdir -p /builds/videolan/vlc
        ln -sfn /opt/jvlc/contrib /builds/videolan/vlc/contrib
        PKGS="ccache"
        ;;
    *)
        echo "usage: $0 win64|linux" >&2
        exit 1
        ;;
esac

missing=""
for p in $PKGS; do
    dpkg -s "$p" >/dev/null 2>&1 || missing="$missing $p"
done
if [ -n "$missing" ]; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -q
    apt-get install -y -q --no-install-recommends --no-upgrade $missing
fi
echo "jvlc: root setup of $1 done"
