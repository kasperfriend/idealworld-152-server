# RUNBOOK — going from a built release to a running server

The GitHub release produced by `.github/workflows/build.yml` intentionally
contains **only what is compiled from this repository**:

```
idealworld-152-server-linux-x86_64/
├── bin/            glinkd  gdeliveryd  gauthd  gfactiond  gamedbd
│                   uniquenamed  logservice  gs
├── lib/            static libraries (libgsio.a, libskill.a, ...) — dev only,
│                   NOT needed at runtime
├── MANIFEST.txt    build info + md5 of every product
└── SHA256SUMS.txt  checksums of bin/ and lib/
```

That is why it looks like "bare files": **configuration files, game data and
the client are not part of the build.** The configs live in this repository
(each daemon's directory), and the game data files (elements.data, maps, …)
are not redistributable here — they must come from a PW-152 era server data
package or be extracted from a matching game client.

This runbook assembles everything into a runnable single-host layout.

---

## 0. Fast path — one-click with the helper scripts

Two scripts in `tools/` automate everything below:

```sh
# in a checkout of this repository, with binaries from ./tools/ci-build.sh
# (dist/) or a downloaded release tarball:
tools/setup-runtime.sh ~/pw [dist|/path/to/release.tar.gz]

# drop your game data package into ~/pw/gs/data/ (see section 5)

cd ~/pw
./pwctl.sh            # start everything (gs is skipped until data exists)
./pwctl.sh status     # process + port table
./pwctl.sh logs gdeliveryd
./pwctl.sh stop
```

`setup-runtime.sh` creates the per-daemon directory layout, copies the
configs from the repo and patches them for a single host:

* every client `address` pointing at the old 172.16.x LAN → `127.0.0.1`
* `gauthd` listen port `9200` → `29200` (matches `GAuthClient`), and its
  ARC4 keys mirrored from gdeliveryd's `[GAuthClient]` (`iseckey`/`oseckey`
  must be swapped between the two ends - shipped values did not match)
* `gdeliveryd` `[GAuthClient] au_cert=false` (no AU billing authority)
* `gamedbd` `zoneid` aligned with `gdeliveryd` (`1`)
* missing WDB `tables=` entries appended (GetStorage returns NULL - and the
  daemon crashes - for any table not listed; the shipped lists were missing
  20 gamedbd tables and 2 uniquenamed tables)
* `/export/...` absolute paths → local dirs, logservice logdir created
* `gs.conf` data paths `/home/cui/nn/` → `./data/`, map aliases
  `GTEST` → `localhost`
* required cwd files staged: `gamedbd/serverlist.sev` (synthesized single
  zone), `gdeliveryd/{auctionid.txt (from the repo), webtradeid.txt,
  sysauctionlist.txt, domain.sev, domain2.sev, filters}`,
  `gfaction/filters`, `gs/restart_zhang` hook stub

`pwctl.sh` starts each daemon as its own process group from its own
directory (listeners first, then connectors, gs worlds last - see
`gs/worlds.list`), keeps pidfiles in `pids/`, stdout/stderr in `logs/`,
shows listening-port state, and stops in reverse order (`gamedbd` /
`uniquenamed` get SIGUSR1 for a clean DB checkpoint). `gs` starts one
process per world listed in `gs/worlds.list`.

> Note for 64-bit builds: this tree originally targeted 32-bit x86. Two
> runtime bugs were fixed to make the daemons start on x86_64: the MMX
> `mppc` compressor truncated 64-bit pointers (`cnet/io/mppc.h`, now plain
> C on non-i386), and per-daemon `-DUSE_HASH_MAP` created mixed
> `hash_map`/`std::map` layouts of shared inline types (removed from the
> daemon Makefiles). `gauthd` also learned the newer `AnnounceZoneid3`
> announcement that gdeliveryd sends. If you use release binaries built
> from an older commit, rebuild from current `main`.

---

## 0b. Running on Windows

The same tree builds as native Win64 executables (`tools/ci-build-win.sh`,
GitHub workflow **Build Windows**), producing all eight daemons:

```
glinkd.exe  gdeliveryd.exe  gauthd.exe   gfactiond.exe
gamedbd.exe uniquenamed.exe logservice.exe gs.exe
```

Everything below §0b in this runbook (config patches, data files, ports,
accounts) applies unchanged — only how you get the binaries, satisfy their
DLL dependencies and start/stop them is different.

### Getting the binaries

* **One-click package (easiest).**  A manual *Build Windows* run publishes
  **`idealworld-152-server-windows-oneclick.zip`**: the 8 `.exe` files in
  their per-daemon runtime layout with the configs already patched for
  127.0.0.1, the mingw runtime DLLs they import, the launchers
  `START-ALL.BAT` / `START-GS.BAT` / `STOP-ALL.BAT` / `STATUS.BAT`, and the
  plain-text guides `HOWTO-PLAY.TXT` (3 steps: what to add, what to run, how
  to log in) and `README-FIRST.TXT`.  Unpack it and double-click
  `START-ALL.BAT`.  The same `HOWTO-PLAY.TXT` is also published next to the
  archive as its own release asset, so you can read it before downloading.
  The archive is produced by `tools/package-win.sh [binaries] [output.zip]`,
  which you can also run yourself once you have the `.exe` files.  It still
  does not contain the game data or the client — see the table at the end of
  this section.
* **From CI.** Every *Build Windows* run (push to the `Windows` branch, or a
  pull request against `main`) uploads the artifact
  `dist-win-native-<run-number>` (MSYS2/mingw64 build), which contains both
  the one-click zip and the bare binaries.  A GitHub **Release** is published
  only for manual (*Run workflow*) runs of that job.
* **Build it on Windows yourself** — install [MSYS2](https://msys2.org), then
  in an **MSYS2 MINGW64** shell:

  ```sh
  pacman -S --needed make zip unzip mingw-w64-x86_64-gcc \
                     mingw-w64-x86_64-openssl mingw-w64-x86_64-pcre
  cd /path/to/idealworld-152-server
  ./tools/ci-build-win.sh                        # -> dist-win/bin/*.exe
  ./tools/package-win.sh dist-win pw-server.zip  # -> the one-click archive
  ```

  (`ci-build-win.sh` also has a `ZIG=…` mode that cross-compiles the same
  targets from Linux; it links *stubs* for pcre/openssl instead of the real
  libraries, so use it for build testing, not for a server you intend to run.)

### Runtime DLLs (not packaged in the bare binaries!)

The mingw64 link line is `-lws2_32 -lwinpthread -lbcrypt -lpsapi -lpcre
-lcrypto` plus the C++ runtime.  `tools/package-win.sh` derives each
binary's import table with `objdump` and copies exactly those libraries from
the mingw `bin` directory into the package root (`START-ALL.BAT` prepends
that root to `PATH`); typically:

```
libwinpthread-1.dll   libgcc_s_seh-1.dll   libstdc++-6.dll
libcrypto-3-x64.dll   libpcre-1.dll
```

If you use the bare `.exe` files instead of the one-click zip, copy the same
files next to them (they live in `C:\msys64\mingw64\bin`) or add that
directory to `PATH`.  `objdump -p gs.exe | grep "DLL Name"` lists what your
own build really imports.

### Assembling the tree

`tools/setup-runtime.sh` accepts the Windows binaries (`dist-win/`, an
unpacked `…-windows-x86_64` release directory or its `.tar.gz`) and stages
them as `.exe` next to the patched configs, so run it once from the MSYS2
shell:

```sh
cd /path/to/idealworld-152-server
./tools/setup-runtime.sh /c/pw dist-win
```

After that you do not need MSYS2 any more: `C:\pw\gauthd\gauthd.exe`,
`C:\pw\glinkd\glinkd.exe`, … are ordinary Win64 programs.  The one-click
zip is exactly this tree plus the launchers, the guides and the DLLs.

### Starting the daemons

Each daemon must be started **from its own directory** (all config paths are
relative) — that is what `START-ALL.BAT` does.  By hand, in `cmd.exe`, one
window per daemon, in this order:

```bat
cd /d C:\pw\gauthd      & gauthd.exe gauthd.conf
cd /d C:\pw\gamedbd     & gamedbd.exe gamesys.conf
cd /d C:\pw\uniquenamed & uniquenamed.exe uniquenamed.conf
cd /d C:\pw\gfaction    & gfactiond.exe gamesys.conf
cd /d C:\pw\gdeliveryd  & gdeliveryd.exe gamesys.conf
cd /d C:\pw\glinkd      & glinkd.exe gamesys.conf 1
cd /d C:\pw\logservice  & logservice.exe logservice.conf
cd /d C:\pw\gs          & gs.exe gs01
```

`./pwctl.sh` also works from the MSYS2 shell (it starts the `.exe` daemons,
keeps pidfiles/logs and prints the port table), but see the stopping caveat
below.

### Stopping

There is no cross-process `SIGUSR1` on Windows: `kill()` in
`win32/winposix.cpp` ignores the pid and only raises the signal inside the
calling process, so the clean-checkpoint stop that `pwctl.sh` uses for
`gamedbd`/`uniquenamed` cannot be delivered.  `STOP-ALL.BAT` therefore uses
`taskkill`.  You can also close a daemon's console window (the shim maps
`CTRL_CLOSE_EVENT` to `SIGTERM`, `Ctrl-C` to `SIGINT` — `ctrl_handler` in
`win32/winposix.cpp`).  The embedded WDB storage then recovers from its logs
on the next start.

### Windows-specific caveats

* **One world per `gs.exe`.**  `fork()` does not exist on Windows, so the
  multi-server startup path is compiled out (`cgame/gs/start.cpp` prints
  *"Windows cannot fork sibling servers"*); start `gs.exe gs01`,
  `gs.exe arena01`, … as separate processes (`START-GS.BAT <world>`).
* **No AF_UNIX.**  `win32/winposix.cpp` maps `PF_UNIX` sockets onto
  `AF_INET` (*"AF_UNIX unsupported"*), so the same-host inter-world channel
  (`[MsgUNIXSession]`, `[MsgReceiverUNIX_*]` in `gs.conf`) cannot work on
  Windows.  A single world is unaffected; several worlds on one host cannot
  exchange messages.
* **`/tmp/...` config entries** (`mtrace = /tmp/m_trace.link` and friends)
  resolve to `<drive>:\tmp\...`; create that directory or comment the entries
  out.
* **`gs` startup probe.**  `gs` checks that its directory is writable before
  initializing.  The original check shelled out to `/bin/touch`, which does
  not exist under `cmd.exe`; the `WIN32` build now probes with `fopen()`
  instead.  Binaries built *before* that fix abort immediately with
  *"文件系统不可写…"* unless a `\bin\touch.bat` exists on the drive.
* **Configs are GBK-encoded** — edit them with an editor that preserves bytes.
* **Firewall.**  Open TCP 9001 for the game client, plus the 29xxx/11100-11101
  ports if the daemons run on different hosts or players connect from the LAN.

### What you still have to supply (Windows or Linux)

The build produces **executables only**.  These are not in the repository and
not in any release:

| Missing piece | Where to get it |
| --- | --- |
| Game data: `elements.data`, `tasks.data`, `dyn_tasks.data`, `world_targets.sev`, `aipolicy.data`, `npcgen.data`, `precinct.sev`, `region.sev`, `path.sev`, `movemap/` `watermap/` `airmap/` and the per-map directories (`world/`, `b01/`, `a01/`, `a02/`, `a05/`, `a06/`, `a07/`) | A PW-152 era server data package, or extracted from a matching client.  Put it in `gs\data\`.  Without it the network daemons run but `gs` cannot start (§5). |
| A game client | A PW-152 era client, patched to point at your `glinkd` host:port (9001).  `glinkd` only *announces* its protocol version in the login challenge (`version` is parsed as **hex**: `10204` in `gamesys.conf`, `804` in the legacy `glinkd.conf`) — the client decides whether it matches. |

What you do **not** need: an account/billing (AU) server.  `gauthd`'s
`UserLogin` handler always answers `ERR_SUCCESS` with `blIsGM=1`
(`cnet/gauthd/userlogin.hrp`) — any account name and password logs in, as a
GM.  Its `./dbhome` storage holds billing/session data, not credentials.

---

## 1. Host prerequisites

(Windows hosts: see §0b instead — the rest of this section is Linux-specific.)


* x86_64 Linux (built on Ubuntu 24.04 runners — a similarly recent distro
  with glibc ≥ 2.35 is safest).
* Runtime shared libraries:

  ```sh
  sudo apt-get install -y libssl3 libpcre3     # Debian/Ubuntu
  ```

* Check what a binary actually needs on your host:

  ```sh
  ldd bin/gs      # must show no "not found"
  ```

## 2. Unpack and verify

```sh
tar -xzf idealworld-152-server-linux-x86_64.tar.gz
cd idealworld-152-server-linux-x86_64
sha256sum -c SHA256SUMS.txt
```

## 3. Assemble the runtime tree

> This section is what `tools/setup-runtime.sh` does automatically (see
> section 0); it is kept as a reference for manual setups.

Each daemon is started **from its own directory**, next to its config file
and its `./dbhome` storage directory (all paths in the configs are relative).
The `cnet/startgame` and `cnet/begingame` scripts in this repo show the
original procedure this layout mirrors.

From a checkout of this repository (`$SRC`) and the unpacked release
(`$REL`):

```sh
SRC=/path/to/this/repo
REL=/path/to/unpacked/release
mkdir -p pw && cd pw

# one directory per daemon: binary + config + local db storage
for d in gauthd gdeliveryd glinkd gfaction gamedbd uniquenamed logservice gs; do
  mkdir -p $d
done

cp $REL/bin/gauthd        gauthd/
cp $REL/bin/gdeliveryd    gdeliveryd/
cp $REL/bin/glinkd        glinkd/
cp $REL/bin/gfactiond     gfaction/
cp $REL/bin/gamedbd       gamedbd/
cp $REL/bin/uniquenamed   uniquenamed/
cp $REL/bin/logservice    logservice/
cp $REL/bin/gs            gs/

# configs ship in the repo, not in the release
cp $SRC/cnet/gauthd/gauthd.conf            gauthd/
cp $SRC/cnet/gdeliveryd/gamesys.conf       gdeliveryd/
cp $SRC/cnet/glinkd/gamesys.conf           glinkd/
cp $SRC/cnet/gfaction/gamesys.conf         gfaction/
cp $SRC/cnet/gamedbd/gamesys.conf          gamedbd/
cp $SRC/cnet/uniquenamed/uniquenamed.conf  uniquenamed/
cp $SRC/cnet/logservice/logservice.conf    logservice/
cp $SRC/cgame/gs/gs.conf                   gs/
cp $SRC/cgame/gs/gs2.conf                  gs/     # second world, if used
cp $SRC/cgame/gs/gmserver.conf             gs/
cp $SRC/cgame/gs/gsalias.conf              gs/
cp $SRC/cgame/gs/ptemplate.conf            gs/
cp $SRC/cgame/gs/rare_item.conf            gs/
chmod +x */g*  */logservice 2>/dev/null || true

# embedded-storage directories (relative ./dbhome / ./dbhomewdb, see [storage]
# and [storagewdb] sections in each conf)
for d in gauthd gdeliveryd glinkd gfaction gamedbd uniquenamed; do
  mkdir -p $d/dbhome/dbdata  $d/dbhome/dblogs  $d/backup
  mkdir -p $d/dbhomewdb/dbdata $d/dbhomewdb/dblogs $d/backupwdb
done
```

## 4. Edit the configs for a single host

The checked-in configs are from the original LAN (172.16.2.x). Every
`address =` that points at another machine must be repointed to
`127.0.0.1` (or your real IP for multi-host setups). Specifically:

| File | Section(s) | Change |
| --- | --- | --- |
| `gdeliveryd/gamesys.conf` | `GAuthClient` (172.16.2.114), `GAntiCheatClient`, `LogclientClient`, `LogclientTcpClient` | → `127.0.0.1` |
| `glinkd/gamesys.conf` | `GAntiCheatClient`, `LogclientClient`, `LogclientTcpClient` | → `127.0.0.1` |
| `gfaction/gamesys.conf` | `GFactionDBClient` (172.16.2.106), `LogclientClient/Tcp` | → `127.0.0.1` |
| `gamedbd/gamesys.conf`, `uniquenamed/uniquenamed.conf` | `LogclientClient`, `LogclientTcpClient` | → `127.0.0.1` |
| `gs/gmserver.conf` | `GProviderClient0/1/2` (172.16.2.118) | → `127.0.0.1` |
| `gs/gs.conf` | `[Template]` `itemDataFile` etc. (`/home/cui/nn/…`) | → your data paths (see §5) |

Port alignment gotchas (two conf families exist in the repo: the private
`*.conf` set using 9xxx ports and the `gamesys.conf` set using 29xxx — use
**one matched set**, this runbook uses `gamesys.conf` everywhere):

* `gauthd/gauthd.conf` listens on **9200**, but `gdeliveryd/gamesys.conf`
  `GAuthClient` connects to **29200**. Either change the `port` in
  `gauthd.conf` to `29200`, or point `GAuthClient` at `9200`.
* `GAntiCheatClient` refers to the `gacd` anti-cheat daemon, which this
  build does not produce. The daemons simply retry the connection forever;
  it is harmless, but you can point it at `127.0.0.1` to keep logs quiet.

## 5. Supply the game data (not in this repository!)

`gs` reads data files referenced by `gs.conf`. Without them the network
daemons in §6 still run and talk to each other, but `gs` cannot start.

| gs.conf key | File(s) needed |
| --- | --- |
| `itemDataFile` | `elements.data` |
| `QuestPackage` / `QuestPackage2` | `tasks.data`, `dyn_tasks.data` |
| `GlobalData` | `world_targets.sev` |
| `PolicyData` | `aipolicy.data` |
| `NPCGenFile` | `npcgen.data` (per map `base_path`) |
| `RegionFile` / `RegionFile2` | `precinct.sev`, `region.sev` (per map) |
| `PathFile` | `path.sev` (per map) |
| `[MoveMap]` | `movemap/`, `watermap/`, `airmap/` directories |
| `base_path` per `[World_*]` | map directories `world/`, `b01/`, `a01/`, `a02/`, `a05/`, `a06/`, `a07/` each containing a `map/` subdirectory |

These come from a PW-152 era server data package or from the client of the
same version. Place them under e.g. `gs/data/…` and fix the paths in
`gs.conf` (`[Template]` entries are absolute paths like `/home/cui/nn/…` in
the sample).

## 6. Start order

Mirrors `cnet/begingame`. Run each line from the daemon's own directory:

```sh
( cd gauthd      && ./gauthd gauthd.conf &        ) # auth / billing front
( cd gdeliveryd  && ./gdeliveryd gamesys.conf &   ) # central dispatcher
( cd glinkd      && ./glinkd gamesys.conf 1 &     ) # client gateway (link 1)
( cd gfaction    && ./gfactiond gamesys.conf &    )
( cd gamedbd     && ./gamedbd gamesys.conf &      ) # role/char database
( cd uniquenamed && ./uniquenamed gamesys.conf &  ) # unique-name service
( cd logservice  && ./logservice logservice.conf &) # optional, keeps logs
# one process per world defined in gs.conf (gs01, gs02, arena01…, is01…):
( cd gs && ./gs gs01 & )
```

`gs` usage (from `cgame/gs/start.cpp`):
`./gs <servername> [gs.conf] [gmserver.conf] [gsalias.conf]` — the
`servername` must match a `[World_<name>]` / `[Instance_<name>]` section and
an entry in `world_servers` / `instance_servers` in `gs.conf`.

`glinkd` usage: `./glinkd <conf> <section_num>` where the number selects the
`[GLinkServer<N>]` section (that is the trailing `1` above).

Use `tail -f` / `netstat -ltnp` to confirm each daemon bound its port
before starting the next.

## 7. Port map (gamesys.conf set)

| Port | Daemon | Purpose |
| --- | --- | --- |
| 9001 | glinkd | **game client entry point** (`version = 10204` hex in `gamesys.conf`; the legacy `glinkd.conf` set says `804`) |
| 29100 | gdeliveryd | GDeliveryServer (glinkd ↔ gdeliveryd) |
| 29200 | gauthd | auth (see §4 port alignment) |
| 29300 | gdeliveryd | GProviderServer — gs connects here |
| 29301 | glinkd | GProviderServer1 — gs connects here |
| 29400 | gamedbd | role database service |
| 29401 | uniquenamed | unique-name service |
| 29500 | gfactiond | faction service |
| 11100/11101 | logservice | UDP/TCP log sink |

## 8. Connecting and accounts

* Point a PW-152 era client at the glinkd port (patch the client's server
  list / `serverlist` the usual way for that client generation).
* Auth goes through `gauthd`, which keeps accounts in its local
  `./dbhome` embedded database. `gamedbd` ships with management
  subcommands (run with no extra argument after the conf file to list
  them): `importclsconfig`, `exportrolelist`, `printlogicuid`,
  `printunamerole`, and friends — useful for inspecting/migrating role
  data in `gamedbd/dbhomewdb`.

## 9. Stopping

Same as `cnet/killgame`:

```sh
killall -9 gauthd gdeliveryd glinkd gfactiond gs
killall -SIGUSR1 gamedbd uniquenamed   # SIGUSR1 = clean checkpoint then exit
```

## 10. Troubleshooting notes

* `gs` refuses to start in a read-only directory — it touches a file named
  `foo` in its cwd on startup as a writability check.
* `gs` forces `TZ=Asia/Shanghai` when the host is already UTC+8; run it
  under the same timezone your data package expects.
* Chinese comments in configs/sources are GBK-encoded — do not "fix" them
  as UTF-8, and edit configs with an editor that leaves bytes alone.
* `mtrace = /tmp/m_trace.*` entries in configs write trace files to `/tmp`.
* The release binary `gamedbd` is built from the `gamedbd.wdb` (WDB
  storage) target; it reads the `[storagewdb]` section of its config, the
  classic BDB `[storage]` section is the legacy variant.
* If a daemon exits immediately, run it in the foreground without `&` —
  every daemon prints its config-parsing errors to stdout/stderr.
