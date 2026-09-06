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
**[RUNBOOK.md](RUNBOOK.md)** for how to assemble a runnable server from a
release tarball: layout, config edits, required data files, start order and
port map.
