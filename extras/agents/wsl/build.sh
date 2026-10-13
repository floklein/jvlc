#!/bin/bash
# Configure (first time) and build one worktree mirror. Run by `jvlc.ps1 build`:
#   build.sh <win64|linux> <key> <install-dir|-> [--reconfigure] [target...]
# The source mirror is $JVLC_WORK/<key>/src (kept in sync by jvlc.ps1), the
# build dir $JVLC_WORK/<key>/build-<platform>.
# win64: cross build with the CI win64 toolchain and the shared contribs, same
#        meson flags as extras/package/win32/build.sh -m (debug), then
#        `meson install --only-changed` to <install-dir> (a Windows folder).
# linux: native build with the CI Debian Qt, like the CI debian-meson job.
set -e
. "$(dirname "$0")/env.sh"

PLATFORM="$1"; KEY="$2"; INSTALL="$3"
shift 3 || die "usage: $0 <win64|linux> <key> <install-dir|-> [--reconfigure] [target...]"
check_key "$KEY"
RECONFIGURE=
if [ "$1" = "--reconfigure" ]; then RECONFIGURE=1; shift; fi

S="$JVLC_WORK/$KEY/src"
B="$JVLC_WORK/$KEY/build-$PLATFORM"
[ -f "$S/meson.build" ] || die "no source mirror in $S (run jvlc.ps1 build, not this script)"
lock_tree "$KEY" "$PLATFORM"

setup_win64()
{
    TRIPLET=x86_64-w64-mingw32
    CONTRIB="$JVLC_ROOT/contrib/$TRIPLET"
    [ -f "$JVLC_ROOT/contrib/.quicktest" ] || die "contribs missing: run jvlc.ps1 setup"

    # Same as build.sh for gcc + msvcrt, debug, meson; -Dalarm= as build.sh
    # does for meson test builds. No installers (nsis, wix).
    CPPFLAGS="-D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0601 -DWINVER=0x0601"
    CFLAGS="$CPPFLAGS -g -Dalarm="
    mkdir -p "$B"
    cat > "$B/crossfile.meson" <<EOF
[binaries]
c = ['ccache', '/usr/bin/$TRIPLET-gcc']
cpp = ['ccache', '/usr/bin/$TRIPLET-g++']
strip = '/usr/bin/$TRIPLET-strip'
pkg-config = '/usr/bin/pkg-config'
windres = '/usr/bin/$TRIPLET-windres'
cmake = '/usr/bin/cmake'
qmake6 = '$CONTRIB/bin/qmake6'
cygpath = '/usr/bin/wslpath'

[properties]
pkg_config_libdir = ['/usr/$TRIPLET/lib/pkgconfig', '/usr/lib/$TRIPLET/pkgconfig']

[host_machine]
system = 'windows'
cpu_family = 'x86_64'
endian = 'little'
cpu = 'x86_64'
EOF
    if [ -x "/opt/tools/bin/$TRIPLET-luac" ]; then
        sed -i "/^cygpath/a luac = '/opt/tools/bin/$TRIPLET-luac'" "$B/crossfile.meson"
    fi

    # -static-libstdc++: VLC loads plugins with LOAD_LIBRARY_SEARCH_SYSTEM32,
    # so a C++ plugin cannot find libstdc++-6.dll next to vlc.exe (build.sh
    # does the same for clang).
    meson setup "$B" "$S" ${RECONFIGURE:+--reconfigure} \
        -Dc_args="$CFLAGS" -Dcpp_args="$CFLAGS" -Dcpp_link_args=-static-libstdc++ \
        -Dcmake_prefix_path="$CONTRIB" \
        -Dlua=enabled -Dflac=enabled -Dtheoradec=enabled -Dtheoraenc=enabled \
        -Davcodec=enabled -Dmerge-ffmpeg=true \
        -Dlibass=enabled -Dshout=enabled -Dgoom2=enabled \
        -Dsse=enabled -Dzvbi=enabled -Dtelx=disabled -Ddvdread=enabled \
        --prefer-static --buildtype debugoptimized -Dnls=disabled \
        -Dqt=enabled -Dskins2=enabled -Dupdate-check=enabled -Dcaca=enabled \
        -Dtests=enabled -Dnsis=disabled -Dwix=disabled \
        --prefix="$INSTALL" \
        --cross-file "$B/crossfile.meson" \
        --cross-file "$CONTRIB/share/meson/cross/contrib.ini"
}

setup_linux()
{
    # -Wno-error=format-overflow: the image's gcc 15 turns an upstream
    # warning of test/src/input/common.c into an error (via -Werror=format).
    meson setup "$B" "$S" ${RECONFIGURE:+--reconfigure} \
        --buildtype debugoptimized \
        -Dc_args=-Wno-error=format-overflow \
        -Dqt=enabled -Dtests=enabled -Dextra_checks=true
}

if [ -n "$RECONFIGURE" ] || [ ! -f "$B/build.ninja" ]; then
    info "configuring $B"
    if [ "$PLATFORM" = win64 ] && [ -z "$RECONFIGURE" ]; then rm -rf "$B"; fi
    "setup_$PLATFORM"
fi

info "building $B with -j$JVLC_JOBS ${*:-(all)}"
meson compile -C "$B" -j "$JVLC_JOBS" "$@"

if [ "$PLATFORM" = win64 ] && [ "$INSTALL" != - ]; then
    bash "$(dirname "$0")/deploy-win64.sh" "$KEY" "$INSTALL"
fi
info "build OK"
