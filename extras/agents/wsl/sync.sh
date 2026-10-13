#!/bin/bash
# Apply a sync batch prepared by jvlc.ps1 to the worktree mirror:
#   sync.sh <key> <worktree (WSL path)> <stem>
# <stem>.tar: a full tree (first sync), <stem>.lst: changed paths,
# <stem>.del: deleted paths (LF separated, relative to the worktree).
# Copies keep their mtimes, so ninja rebuilds what changed.
set -e
. "$(dirname "$0")/env.sh"

KEY="$1"; WT="$2"; STEM="$3"
check_key "$KEY"
S="$JVLC_WORK/$KEY/src"
mkdir -p "$S"
if [ -f "$STEM.tar" ]; then
    # first sync: `git archive` of HEAD; mtimes are the commit time, unless
    # a build dir already exists (lost manifest): then they must look new.
    TOUCH=
    ls -d "$JVLC_WORK/$KEY"/build-* >/dev/null 2>&1 && TOUCH=--touch
    tar -xf "$STEM.tar" -C "$S" $TOUCH
fi
if [ -s "$STEM.lst" ]; then
    tar -cf - -C "$WT" --verbatim-files-from -T "$STEM.lst" | tar -xf - -C "$S"
fi
if [ -s "$STEM.del" ]; then
    (cd "$S" && xargs -r -d '\n' rm -f -- < "$STEM.del")
fi
