#!/bin/bash
# One-time (per contrib SHA) setup of the shared Windows contribs in the
# jvlc-win64 distro. Run by `jvlc.ps1 setup`, as the videolan user:
#   setup-win64.sh <contrib-sha> <contrib-tarball>
# <contrib-tarball> is `git archive <contrib-sha> contrib extras/tools` of the
# worktree. Reproduces what extras/package/win32/build.sh -a x86_64 -p does in
# the upstream CI `win64` job, then adds QtQuickTest (see below).
set -e
. "$(dirname "$0")/env.sh"

SHA="$1"
TARBALL="$2"
[ -n "$SHA" ] && [ -f "$TARBALL" ] || die "usage: $0 <contrib-sha> <contrib-tarball>"

TRIPLET=x86_64-w64-mingw32
SRC="$JVLC_ROOT/contrib-src/$SHA"
CONTRIB="$JVLC_ROOT/contrib"
PREFIX="$CONTRIB/$TRIPLET"
JOBS="${SETUP_JOBS:-$(nproc)}"

if [ "$(cat "$CONTRIB/SHA" 2>/dev/null)" = "$SHA" ] && [ -f "$CONTRIB/.quicktest" ]; then
    info "contribs for $SHA already set up"
    exit 0
fi

mkdir -p "$SRC" "$CCACHE_DIR"
tar -xf "$TARBALL" -C "$SRC"

info "building extras/tools"
mkdir -p "$JVLC_ROOT/tools"
cd "$JVLC_ROOT/tools"
"$SRC/extras/tools/bootstrap"
make -j"$JOBS"
export PATH="$JVLC_ROOT/tools/build/bin:$PATH"

info "fetching the prebuilt contribs of the CI win64 job"
mkdir -p "$CONTRIB/contrib-win64"
cd "$CONTRIB/contrib-win64"
# Same flags as build.sh for gcc + msvcrt, debug
CPPFLAGS="-D__MSVCRT_VERSION__=0x700 -D_WIN32_WINNT=0x0601 -DWINVER=0x0601"
cat > config.mak <<EOF
# This file was automatically generated!

CPPFLAGS := $CPPFLAGS
CFLAGS := $CPPFLAGS -g
CXXFLAGS := $CPPFLAGS -g
PKG_CONFIG := pkg-config
PKG_CONFIG_LIBDIR := /usr/$TRIPLET/lib/pkgconfig:/usr/lib/$TRIPLET/pkgconfig
EOF
"$SRC/contrib/bootstrap" --host=$TRIPLET --prefix="../$TRIPLET" --enable-sections
URL="https://artifacts.videolan.org/vlc/win64/vlc-contrib-$TRIPLET-$SHA.tar.zst"
if [ ! -f ".prebuilt-$SHA" ]; then
    rm -f vlc-contrib-$TRIPLET-latest.tar.zst .qtdeclarative
    make prebuilt PREBUILT_URL="$URL"
    touch ".prebuilt-$SHA"
fi
make -j"$JOBS" tools

# Some contrib .pc files list -lssp. With meson --prefer-static that becomes
# libssp.a, which clashes with the -lssp (libssp.dll.a) VLC's meson.build adds
# itself (multiple definition of __stack_chk_fail). Upstream CI builds gcc with
# autotools, so it never hits this. VLC links ssp anyway: drop it there.
sed -i 's/ -lssp\b//' "$PREFIX"/lib/pkgconfig/*.pc
sed -i 's/"ssp\.a;/"/' "$PREFIX"/lib/cmake/FLAC/targets.cmake

# The CI contribs build qtdeclarative with QT_FEATURE_testlib=OFF, so there is
# no QtQuickTest and meson skips qml_test (tst_FSM). Rebuild qtdeclarative
# alone, from the same rules with testlib on, and install it over the prebuilt
# one. Its dependencies are taken from the prebuilt prefix (-o: never remake).
info "rebuilding qtdeclarative with QtQuickTest"
sed -i 's/-DQT_FEATURE_testlib=OFF/-DQT_FEATURE_testlib=ON/' "$SRC/contrib/src/qtdeclarative/rules.mak"
OLD=""
for p in $(make -s list | sed -n '/^All packages:/,/^All native tools:/p' | grep -v ':' ) \
         $(make -s list | sed -n '/^All native tools:/,$p' | grep -v ':'); do
    case "$p" in qtdeclarative) continue ;; esac
    OLD="$OLD -o .$p -o .dep-$p"
done
make $OLD qtdeclarative
# qmltestrunner (enabled by testlib) wants a host tool the CI image lacks; the
# QuickTest library does not need it.
sed -i 's,^\( *\)add_subdirectory(qmltestrunner),\1#add_subdirectory(qmltestrunner),' \
    qtdeclarative/tools/CMakeLists.txt
make -j"$JOBS" $OLD .qtdeclarative
rm -rf qtdeclarative

echo "$SHA" > "$CONTRIB/SHA"
touch "$CONTRIB/.quicktest"
info "contribs ready in $PREFIX"
