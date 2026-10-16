# Build, test and run

How agents build this VLC fork, run the Qt tests and launch the app on Windows and on Linux. Use these commands as they are. Do not install other toolchains or invent another build.

Everything goes through one PowerShell script, `extras\agents\jvlc.ps1`. Run it from your worktree; it only touches that worktree's build.

## Quick reference

```powershell
extras\agents\jvlc.ps1 build                    # win64 + linux: configure if needed, build everything
extras\agents\jvlc.ps1 build -Qt                # rebuild only the Qt interface module, both platforms
extras\agents\jvlc.ps1 build -Platform win64 -Qt
extras\agents\jvlc.ps1 test                     # Qt model tests + QML tests, win64 and linux
extras\agents\jvlc.ps1 test -Platform linux -Filter '*navigation*'
extras\agents\jvlc.ps1 run -Platform win64   -Shot C:\Users\flo-k.DESKTOP-LHU5KDO\jvlc-shared\shots\<N>\win64.png
extras\agents\jvlc.ps1 run -Platform x11     -Shot ...\x11.png
extras\agents\jvlc.ps1 run -Platform wayland -Shot ...\wayland.png
extras\agents\jvlc.ps1 run -Platform win64 -Keep   # leave it open, then:
extras\agents\jvlc.ps1 shot -Platform win64 -Shot ...\after.png
extras\agents\jvlc.ps1 info                     # where this worktree's build lives
extras\agents\jvlc.ps1 shell -Platform linux    # bash in the build distro
extras\agents\jvlc.ps1 clean                    # drop this worktree's build state
```

- `-Platform` is `win64`, `linux` or `all` (the default) for `build`, `test`, `shell` and `clean`. For `run` and `shot` it is `win64`, `x11` or `wayland`.
- `build -Target <meson target>...` builds only those targets. `-Qt` is short for `-Target qt_plugin`.
- `build -Reconfigure` re-runs `meson setup`. Ninja re-runs it by itself when a `meson.build` changes, so you rarely need this.
- `test` syncs your changes and rebuilds what the tests need, so it does not need a `build` first. A worktree still needs one `build` before its first `test` or `run`.
- `run` starts jVLC, waits for its window, waits `-Wait` seconds (default 5), saves a PNG of the window if `-Shot` is given, then closes jVLC. `-Keep` leaves it open. `-VlcArgs` passes media or options, for example `-VlcArgs 'C:\clips\a.mkv'` or `-VlcArgs '--qt-compositor=win7'`.
- `run` and `shot` do not rebuild: run `build` (or `build -Qt`) first.
- Logs: `jvlc-shared\builds\<key>\run-<platform>.log` (jVLC at `-vv`), `vlc-win64\tests\<test>.log` (win64 tests), `/opt/jvlc/w/<key>/build-linux/meson-logs/testlog.txt` (linux tests).

## Parallelism cap

Several agents build at once on this machine: 12 cores, 32 GB of RAM, of which WSL gets 16 GB.

- Each `build` and `test` uses **4 jobs**. Keep it at 4 while other agents may be building.
- Pass `-Jobs N` or set `$env:JVLC_JOBS` to change it. Use `-Jobs 2` if a build dies with `Killed` or `signal 9` (out of memory). Go above 4 only when you know you are alone.
- Two commands on the same worktree and platform never run at once: the second waits for the first.

## Typical times

Measured with 4 jobs:

| step | win64 | linux |
|---|---|---|
| first `build` of a new worktree (ccache warm from other worktrees) | 100 s | 56 s |
| first `test` of a new worktree (builds the test programs) | 35 s | 25 s |
| `build -Qt` after editing one `.cpp` of the Qt module | 14 s | 6 s |
| `build -Qt` after editing one `.qml` | 27 s | 20 s |
| `test`, nothing to rebuild | 15 s | 8 s |

Busy machines are slower. A build with an empty ccache takes several times longer; that only happens after `setup` or a contrib change.

## How it works

### Where things are

| what | where |
|---|---|
| helper scripts | `extras/agents/jvlc.ps1` and `extras/agents/wsl/*.sh` in the repo |
| shared state | `C:\Users\flo-k.DESKTOP-LHU5KDO\jvlc-shared\`, outside every worktree |
| build distros | WSL distros `jvlc-win64` and `jvlc-linux`. Their disks are `jvlc-shared\wsl\<distro>\ext4.vhdx` |
| CI images they come from | `jvlc-shared\wsl\images\*.tar`, exported Docker images |
| Windows contribs (static Qt 6.8.3, ffmpeg, …) | `/opt/jvlc/contrib/x86_64-w64-mingw32` in `jvlc-win64` |
| ccache | `/opt/jvlc/ccache` in each distro, shared by all worktrees |
| your source mirror | `/opt/jvlc/w/<key>/src` in each distro |
| your build dirs | `/opt/jvlc/w/<key>/build-win64` in `jvlc-win64`, `/opt/jvlc/w/<key>/build-linux` in `jvlc-linux` |
| your Windows install | `jvlc-shared\builds\<key>\vlc-win64\`: `vlc.exe`, DLLs, `plugins\`, `portable\`, `tests\` |
| screenshots | `jvlc-shared\shots\<ticket>\` |

`<key>` is the worktree folder name plus a hash of its path. `jvlc.ps1 info` prints it.

### Toolchains: the upstream CI images

The build reproduces VLC's own CI (`extras/ci/gitlab-ci.yml`) rather than a hand-made toolchain.

- **win64**: image `registry.videolan.org/vlc-debian-win64-posix:20260611225331`, the one the CI `win64` job uses (mingw-w64 gcc 14). The contribs are the CI's prebuilt ones for this tree's contrib SHA (`extras/ci/get-contrib-sha.sh win64`), from `https://artifacts.videolan.org/vlc/win64/`. The meson options are those of `extras/package/win32/build.sh -m` in debug (`debugoptimized`), plus `-Dtests=enabled`, without the installers, and with `-static-libstdc++` (VLC loads plugins with `LOAD_LIBRARY_SEARCH_SYSTEM32`, so a C++ plugin cannot find `libstdc++-6.dll`).
- **linux**: image `registry.videolan.org/vlc-debian-unstable:20260611225331`, the one the CI `debian-meson` job uses (Debian Qt 6.10.2). Options: `--buildtype debugoptimized -Dqt=enabled -Dtests=enabled -Dextra_checks=true -Dc_args=-Wno-error=format-overflow`. The last one works around an upstream warning that this image's gcc 15 turns into an error.
- Each image is imported once as a WSL distro (`wsl --import`). The builds run in WSL2 on ext4, without Docker. Docker Desktop is only needed by `setup`, to pull the images.

Local deviations from upstream, all in the shared contribs (outside the repo):

1. The CI contribs build `qtdeclarative` without QtQuickTest, so meson would silently skip the QML test (`tst_FSM`) on Windows. `setup` rebuilds `qtdeclarative` alone, from the same contrib rules with `QT_FEATURE_testlib=ON`, and installs it over the prebuilt one.
2. The prebuilt contribs keep a few absolute paths of the CI machine (`/builds/videolan/vlc/contrib/...`). A symlink in `jvlc-win64` maps them to `/opt/jvlc/contrib`.
3. `flac.pc`, `libbluray.pc` and FLAC's CMake config list `-lssp`. With `--prefer-static`, meson turns that into `libssp.a`, which clashes with the `-lssp` VLC's `meson.build` adds. `setup` removes it from those files. Upstream builds gcc with autotools, so it never hits this.

### Source mirror

The build does not read your worktree through `/mnt/c`, which costs about 2 ms per file and would make every ninja run slow. Each `build` and `test` lists the files git knows (tracked, plus untracked files that are not ignored), compares their size and mtime with the last sync, and copies only the changed ones into `/opt/jvlc/w/<key>/src`, keeping their mtimes. Ninja then rebuilds exactly what changed. The first sync of a worktree is a `git archive` of `HEAD` plus your local changes.

- Compiler errors show mirror paths: `../src/modules/gui/qt/foo.cpp` or `/opt/jvlc/w/<key>/src/modules/gui/qt/foo.cpp` is `modules\gui\qt\foo.cpp` in your worktree.
- Never edit the mirror: the next sync overwrites or ignores your edit.
- Files ignored by git are not synced.

### ccache across worktrees

Every worktree has the same layout under `/opt/jvlc/w/<key>/` (`src` next to `build-<platform>`), and ccache runs with `base_dir=/opt/jvlc/w`. So the compiler commands are the same in every worktree, and a new worktree's first build mostly hits the cache of the previous ones.

### win64 runs on Windows

- `build -Platform win64` ends by installing into `jvlc-shared\builds\<key>\vlc-win64\` (`deploy-win64.sh`). The first time, it runs `meson install --strip` into a staging dir on ext4 and copies the result. Afterwards it copies, stripped, only the installed files whose build output changed, so after `-Qt` only `plugins\gui\libqt_plugin.dll` (and its import library) is copied. It also copies the gcc runtime DLLs `libvlccore.dll` needs.
- A `portable\` folder sits next to `vlc.exe`, so jVLC keeps its config there instead of `%APPDATA%\vlc`: each worktree has its own. Delete `portable\*` to reset it.
- `run` starts jVLC with `--no-qt-privacy-ask --no-qt-updates-notif` and, unless `-Audio` is given, `--aout=dummy`: it is silent by default. Only pass `-Audio` when a check needs a real audio output, and then mute that process in the volume mixer. The compositor is jVLC's default (`dcomp`). Pass `-VlcArgs '--qt-compositor=dcomp'` to make sure no fallback is used.

### Tests

- The suite is every meson test in suite `qt`: `base_model_test`, `test_qt_ml_model`, `test_qt_navigation_model`, `test_qt_renderer_manager`, `test_qt_thread_runner`, `test_qt_dialog_model`, and one test per `tst_*.qml` registered in `qml_tests` in `modules/gui/qt/meson.build`. New tests registered there run on both platforms with no change here. How to write one: `jvlc-shared\notes\codebase\tests.md`.
- **linux**: `meson test --suite qt` in the build dir, with `VLC_TEST_TIMEOUT=60` as in CI. The C++ tests need no display, and `qml_test` forces the `offscreen` platform.
- **win64**: the test executables are cross-built in WSL, copied to `vlc-win64\tests\` (with `vlc-preparser.exe`) and run natively on Windows. They use the installed `libvlc*.dll` and `plugins\` (`VLC_LIB_PATH`). The QML test gets `-input <worktree>\modules\gui\qt\tests\tst_*.qml`. QTest output goes to `vlc-win64\tests\<test>.log` (`-o`), since it does not reach a redirected stdout there.
- Upstream never ran these tests on Windows. Three repo changes made them build and pass: `vlc_static_modules[]` of an executable is now loaded on Windows (`src/modules/bank.c`), `qml_test` and `test_qt_navigation_model` link the static Qt like the other tests, and `vlc_stub_modules.cpp` keeps plugin paths set by the caller. Two tests that waited a fixed 1 ms or 30 ms now wait for the condition, as they failed on Windows timers or under load. Write new tests the same way: wait for a condition (`tryCompare`, `QTRY_VERIFY`), never a fixed delay, since several agents load the machine at once.
- A new C++ test in `modules/gui/qt/meson.build` needs `'link_args': qt_link_args` like the others, or it does not link on win64.
- Tests that share one stand-in VLC (`VLCTestingEnv`) share its playlist and player. Reset both in `cleanup()`, which also runs after a failure: stop the playlist, clear it, and wait until the player has no media (see `resetQueue()` in `test_ml_model.cpp`). A player still stopping the previous test's media moves the next test's current item.

### Linux GUI through WSLg

- `run -Platform x11` sets `QT_QPA_PLATFORM=xcb` with `DISPLAY=:0`. `run -Platform wayland` sets `QT_QPA_PLATFORM=wayland` with the WSLg Wayland socket and no `DISPLAY`. Both run `bin/vlc-static -I qt` from the build dir, on the GPU through Mesa's d3d12 driver.
- The config lives in `/opt/jvlc/w/<key>/home` (`XDG_*` dirs), not in `~/.config/vlc`.
- WSLg's XWayland is older than 21.1. jVLC therefore falls back from its X11 compositor to the dummy one (log: `running a broken XWayland version`), and WSLg shows nothing of what an X11 window draws until the window is resized. `run` and `shot` resize the X11 window by one pixel and back (`x11-nudge.sh`) before the screenshot. This is a WSLg quirk, not a jVLC bug: do not "fix" it in jVLC.
- WSLg shows Linux windows on the Windows desktop through `msrdc.exe`, titled `jVLC (jvlc-linux)` (or `<media> · jVLC (jvlc-linux)`). `run` picks the window that appeared after the launch. `shot -Platform x11|wayland` picks the first such window, so it is ambiguous while other agents run Linux jVLC: prefer `run -Shot`.

### Screenshots

Screenshots use `PrintWindow` with `PW_RENDERFULLCONTENT`: DWM renders the window's own content, DirectComposition and WSLg windows included. Other windows covering it do not matter, and the focus is not taken. The window must not be minimized (`shot` restores it without activating it).

## Setup (once per machine)

Already done on this machine. To redo it, or after a contrib change:

```powershell
extras\agents\jvlc.ps1 setup
```

`setup` is idempotent. It:

1. imports the two CI images as WSL distros if they are missing, after pulling them with Docker (so Docker Desktop must be running);
2. installs, from each image's own Debian release, `ccache` in both distros and `qt6-wayland`, `qt6-svg-plugins`, `xdotool` and `libmedialibrary-dev` in `jvlc-linux`. For sid it uses the snapshot of the image's date, so Qt is never upgraded;
3. downloads the prebuilt win64 contribs of this tree's contrib SHA, patches them (see above) and adds QtQuickTest (about 30 minutes).

A ticket that changes `contrib/` or `extras/tools/` changes the contrib SHA. No prebuilt contribs exist for the new SHA, so that ticket must build the contribs, which `setup` does not cover.

## Troubleshooting

- `contribs missing: run jvlc.ps1 setup`: the shared contribs are not installed.
- `There is no distribution with the supplied name`: the distros are missing; run `setup`.
- The Linux jVLC has no media library (the Library shows only Home and Network, no `libmedialibrary_plugin.so` in `build-linux/modules`): the build dir was configured before `setup` installed `libmedialibrary-dev`. Run `build -Platform linux -Reconfigure`.
- A build fails after a merge with odd meson errors: run `build -Reconfigure`. If that fails too, run `clean`, then `build`.
- `vlc.exe` says "could not start ... no plugins were found", or a win64 test exits with `-1073741515` (`0xC0000135`, DLL not found): run `build -Platform win64` to refresh the install.
- `Fail to create Process in process_pool` on win64 (a stand-in VLC test fails in `initTestCase`, or `vlc.exe` starts without its playlist): your branch predates the `vlc_pipe()` fix (`src/win32/filesystem.c`), so merge `feat/redesign` and rebuild win64. Before it, every jVLC process named its pipes `\\.\pipe\AnonPipe_0`, `_1`, … Pipe names are global to the machine, so while one jVLC (another agent's `vlc.exe` or test) held those names, every other jVLC failed to spawn `vlc-preparser.exe`. That is why it came and went with the other agents and looked like a load or install problem. Pipes are now named `vlc-pipe-<pid>-<n>`; `Get-ChildItem \\.\pipe\ | ? Name -like 'vlc-pipe*'` lists them.
- Nothing shows up for `run -Platform x11|wayland`: check `run-<platform>.log`. If WSLg itself is stuck, `wsl --shutdown` restarts it, but it also kills every other agent's build: check that none is running first.
