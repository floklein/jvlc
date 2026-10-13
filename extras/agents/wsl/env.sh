# Shared environment of the jvlc agent build distros (jvlc-win64, jvlc-linux).
# Sourced by the other scripts of this directory; see docs/agents/build.md.

# The CI images set these in their Docker config, which `wsl --import` drops.
export PATH="/opt/tools/bin:/opt/tools/libexec:/opt/rust/cargo/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export RUST_HOME=/opt/rust RUSTUP_HOME=/opt/rust/rustup CARGO_HOME=/opt/rust/cargo
export LANG=C.UTF-8

# Everything heavy lives under /opt/jvlc, inside the distro's ext4 disk
# (jvlc-shared\wsl\<distro>\ext4.vhdx on the Windows side).
JVLC_ROOT=/opt/jvlc
# Per-worktree trees: $JVLC_ROOT/w/<key>/src (mirror of the worktree) and
# $JVLC_ROOT/w/<key>/build-<platform> (meson build dir). Both sit at the same
# depth for every worktree, so the relative paths meson passes to the compiler
# are identical and ccache hits across worktrees.
JVLC_WORK="$JVLC_ROOT/w"
# extras/tools built by setup (jvlc-win64 only): its meson must configure and
# build, never the older system one
if [ -d "$JVLC_ROOT/tools/build/bin" ]; then
    export PATH="$JVLC_ROOT/tools/build/bin:$PATH"
fi

export CCACHE_DIR="$JVLC_ROOT/ccache"
export CCACHE_BASEDIR="$JVLC_WORK"
export CCACHE_NOHASHDIR=1
export CCACHE_MAXSIZE=40G
export CCACHE_COMPILERCHECK=content

# Per-agent build parallelism cap (docs/agents/build.md).
: "${JVLC_JOBS:=4}"

die() { echo "jvlc: $*" >&2; exit 1; }
info() { echo "jvlc: $*" >&2; }

# lock_tree <key> <platform>: one build or test at a time per build dir; a
# second one waits.
lock_tree()
{
    mkdir -p "$JVLC_WORK/$1"
    exec 9>"$JVLC_WORK/$1/.lock-$2"
    if ! flock -n 9; then
        info "waiting for another build/test of this worktree ($2)"
        flock 9
    fi
}

# check_key <key>: worktree keys are made by jvlc.ps1; refuse anything else.
check_key()
{
    case "$1" in
        ''|*/*|.*) die "bad worktree key '$1'" ;;
    esac
}
