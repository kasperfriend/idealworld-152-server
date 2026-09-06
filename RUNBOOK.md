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

## 1. Host prerequisites

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
| 9001 | glinkd | **game client entry point** (`version = 804` in the conf) |
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
