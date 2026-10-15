#!/bin/bash
# Build the Qt tests of the win64 build and stage them for a native run:
#   JVLC_FILTER=<name-glob> test-win64.sh <key> <install-dir>
# Copies the test executables to <install-dir>/tests and writes
# <install-dir>/tests/tests.tsv (name, exe, -input file) for jvlc.ps1, which
# runs them on Windows next to the installed libvlc*.dll and plugins.
set -e
. "$(dirname "$0")/env.sh"

KEY="$1"; INSTALL="$2"; FILTER="${JVLC_FILTER:-*}"
check_key "$KEY"
B="$JVLC_WORK/$KEY/build-win64"
[ -f "$B/build.ninja" ] || die "no win64 build: run jvlc.ps1 build -Platform win64"
lock_tree "$KEY" win64

# name <TAB> exe path <TAB> -input argument (QML tests)
meson introspect "$B" --tests | python3 -c '
import fnmatch, json, os, sys
for t in json.load(sys.stdin):
    suites = [s.split(":")[-1] for s in t["suite"]]   # "VLC:qt"
    if "qt" not in suites or not fnmatch.fnmatchcase(t["name"], sys.argv[1]):
        continue
    cmd = t["cmd"]
    arg = os.path.normpath(cmd[cmd.index("-input") + 1]) if "-input" in cmd else ""
    print("\t".join([t["name"], cmd[0], arg]))
' "$FILTER" > "$B/qt-tests.tsv"
[ -s "$B/qt-tests.tsv" ] || die "no qt test matches '$FILTER'"

targets=$(cut -f2 "$B/qt-tests.tsv" | sort -u | sed "s,^$B/,,")
info "building $(echo $targets | wc -w) test executable(s) with -j$JVLC_JOBS"
# shellcheck disable=SC2086
ninja -C "$B" -j "$JVLC_JOBS" $targets modules/libdemux_mock_plugin.dll
# the tests load the installed libvlc*.dll and plugins: refresh them
bash "$(dirname "$0")/deploy-win64.sh" "$KEY" "$INSTALL"
# mock:// (test_qt_conversion_manager) needs demux_mock, which meson does
# not install
mkdir -p "$INSTALL/plugins/demux"
cp -p "$B/modules/libdemux_mock_plugin.dll" "$INSTALL/plugins/demux/"

mkdir -p "$INSTALL/tests"
# libvlc spawns vlc-preparser.exe from the directory of the executable
cp -p "$INSTALL/vlc-preparser.exe" "$INSTALL/tests/"
: > "$INSTALL/tests/tests.tsv.new"
while IFS=$'\t' read -r name exe arg; do
    cp -p "$exe" "$INSTALL/tests/"
    printf '%s\t%s\t%s\n' "$name" "$(basename "$exe")" "$arg" >> "$INSTALL/tests/tests.tsv.new"
done < "$B/qt-tests.tsv"
mv "$INSTALL/tests/tests.tsv.new" "$INSTALL/tests/tests.tsv"
