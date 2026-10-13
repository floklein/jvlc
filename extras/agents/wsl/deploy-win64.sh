#!/bin/bash
# Install the win64 build into its Windows folder:
#   deploy-win64.sh <key> <install-dir>
# First time: `meson install --strip` into a staging dir on ext4 (seconds; the
# same install straight to /mnt/c takes minutes), copied as a whole. Then
# only the installed files whose build output changed since the last install
# are copied, stripped. (Comparing stripped copies does not work: strip
# stamps PE files with the current time.)
set -e
. "$(dirname "$0")/env.sh"

KEY="$1"; INSTALL="$2"
check_key "$KEY"
B="$JVLC_WORK/$KEY/build-win64"
STAGE="$JVLC_WORK/$KEY/stage-win64"
STAMP="$B/.jvlc-installed"
STRIP=x86_64-w64-mingw32-strip

touch "$STAMP.new"
if [ ! -f "$STAMP" ] || [ ! -f "$INSTALL/libvlccore.dll" ]; then
    rm -rf "$STAGE"
    meson install -C "$B" --no-rebuild --strip --quiet --destdir "$STAGE"
    mkdir -p "$INSTALL"
    cp -r "$STAGE$INSTALL/." "$INSTALL/"
    rm -rf "$STAGE"
    info "installed to $INSTALL"
else
    n=0
    while IFS=$'\t' read -r src dst; do
        [ -f "$src" ] && [ "$src" -nt "$STAMP" ] || continue
        mkdir -p "$(dirname "$dst")"
        case "$src" in
            *.dll|*.exe)
                $STRIP -o "$B/.jvlc-strip.tmp" "$src"
                cp "$B/.jvlc-strip.tmp" "$dst" ;;
            *)
                cp "$src" "$dst" ;;
        esac
        n=$((n + 1))
    done < <(meson introspect "$B" --installed | python3 -c '
import json, sys
for src, dst in json.load(sys.stdin).items():
    print(src + "\t" + dst)
')
    rm -f "$B/.jvlc-strip.tmp"
    info "installed $n changed file(s) to $INSTALL"
fi
mv "$STAMP.new" "$STAMP"

# gcc runtime DLLs libvlccore.dll imports (the meson install does not ship
# them); plugins then reuse the copies already loaded
for dll in libssp-0.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
    f=$(x86_64-w64-mingw32-gcc -print-file-name=$dll)
    [ -f "$f" ] || f=/usr/x86_64-w64-mingw32/lib/$dll
    [ "$INSTALL/$dll" -nt "$f" ] || cp "$f" "$INSTALL/"
done
