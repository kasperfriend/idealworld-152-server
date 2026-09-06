# idealworld-152-server

Server source for the idealworld 152 generation. Build everything with:

```sh
sudo apt-get install -y build-essential libssl-dev libpcre3-dev
./tools/ci-build.sh        # products land in dist/bin and dist/lib
```

The GitHub Actions workflow **Build Server** (manual `workflow_dispatch`)
compiles the same tree on a clean runner and publishes the binaries as a
release.

The release contains **only the compiled binaries** — configs, game data
(`elements.data`, maps, …) and the client are not packaged. See
**[RUNBOOK.md](RUNBOOK.md)** for the full path from tarball to running
server.

## One-click launch

```sh
tools/setup-runtime.sh ~/pw [dist|release.tar.gz]   # assemble runtime tree
~/pw/pwctl.sh                                        # start / stop / status
```

`setup-runtime.sh` builds the per-daemon layout, copies + patches the
configs for single-host 127.0.0.1 operation and stages the required cwd
data files. `pwctl.sh` starts all daemons in dependency order, waits for
each listening port, keeps pidfiles/logs, and shuts down cleanly (SIGUSR1
checkpoint for the DB daemons). The game world (`gs`) starts as soon as a
PW-152 data package is dropped into `<runtime>/gs/data/`.
