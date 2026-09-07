#!/usr/bin/env bash
#
# Build the idealworld-152 server from source and stage the results in dist/.
#
# This is the single entry point used by .github/workflows/build.yml, but it is
# meant to be runnable on any x86_64 Linux box:
#
#     sudo apt-get install -y build-essential libssl-dev libpcre3-dev
#     ./tools/ci-build.sh
#
# Products (dist/bin): glinkd, gdeliveryd, gauthd, gamedbd, gfactiond,
#                      uniquenamed, logservice, gs
# Static libs (dist/lib): libperf.a, libgsio.a, libgsPro2.a, libdbCli.a,
#                         liblogCli.a, libskill.a, libTrace.a, libonline.a,
#                         libcommon.a
#
# Environment:
#   JOBS       -j value (default: nproc)
#   DIST_DIR   where to stage the result (default: <repo>/dist)
#   STRICT     0 to keep going after a failing step (default: 1 -> fail the run)
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
DIST_DIR="${DIST_DIR:-$ROOT/dist}"
STRICT="${STRICT:-1}"

STATE="$ROOT/ci-build"
rm -rf "$STATE" "$DIST_DIR"
mkdir -p "$STATE" "$DIST_DIR/bin" "$DIST_DIR/lib"

declare -a STEP_RESULT
FAILED=0

# step <name> <command...>: run one build step, keep its log, remember the result
step() {
	local name="$1"; shift
	echo "::group::$name"
	echo "\$ $*"
	local rc=0
	"$@" > "$STATE/$name.log" 2>&1 || rc=$?
	if [ "$rc" -eq 0 ]; then
		echo "ok"
		printf 'ok\n' > "$STATE/$name.status"
	else
		FAILED=$((FAILED+1))
		printf 'failed (exit %d)\n' "$rc" > "$STATE/$name.status"
		echo "FAILED (exit $rc) - last lines of $STATE/$name.log:"
		grep -E "error:|Error [0-9]+|undefined reference|No such file" \
			"$STATE/$name.log" | head -n 60
		[ -s "$STATE/$name.log" ] || cat "$STATE/$name.log" | tail -n 20
	fi
	echo "::endgroup::"
	STEP_RESULT+=("$name: $(cat "$STATE/$name.status")")
	return 0
}

echo "=== environment ==="
uname -a
gcc --version | head -n1
g++ --version | head -n1
make --version | head -n1
echo "nproc=$(nproc 2>/dev/null || echo '?') JOBS=$JOBS"

# The tree used to carry 32-bit (i386) objects and archives from the original
# build machine.  They must never be reused by a 64-bit build, so drop every
# leftover object/archive before compiling (the build scripts regenerate them).
echo "=== removing stale build artifacts ==="
find cnet cgame cskill -name '*.o' -delete 2>/dev/null
find cnet cgame cskill -name '*.a' -delete 2>/dev/null

# ---------------------------------------------------------------- cnet: libs --
step "cnet-perf"   make -C cnet/perf -j"$JOBS"
step "cnet-io"     make -C cnet/io lib -j"$JOBS"
step "cnet-gamed"  make -C cnet/gamed lib -j"$JOBS"
step "cnet-gdbclient" make -C cnet/gdbclient lib -j"$JOBS"
step "cnet-logclient" make -C cnet/logclient -f Makefile.gs lib -j"$JOBS"
# cnet/shm (libshmmgr.a) is deliberately not built: no daemon in this tree
# links it, and its ptmalloc wrapper (ptmalloctor.h) predates C++11.

# -------------------------------------------------------------- cskill: lib --
step "cskill"      make -C cskill/skill -f ../Makefilelib -j"$JOBS"

# ---------------------------------------------------------- cnet: daemons -----
step "glinkd"      make -C cnet/glinkd -j"$JOBS"
step "gdeliveryd"  make -C cnet/gdeliveryd -j"$JOBS"
step "gauthd"      make -C cnet/gauthd -j"$JOBS"
step "gfaction"   make -C cnet/gfaction -j"$JOBS"
step "gamedbd"     make -C cnet/gamedbd -j"$JOBS"
step "uniquenamed" make -C cnet/uniquenamed -j"$JOBS"
step "logservice"  make -C cnet/logservice -j"$JOBS"

# ------------------------------------------------------------- cgame: gs -----
step "cgame-collision" make -C cgame/collision -j"$JOBS"
step "cgame-cmlib"     make -C cgame/common -j"$JOBS"
step "cmlib-selftest"  make -C cgame/common test -j"$JOBS"
step "cgame-libs"      make -C cgame lib -j"$JOBS"
step "gs"              make -C cgame/gs -j"$JOBS"

# ------------------------------------------------------------- staging --------
echo "=== staging dist/ ==="
copy_pair() { # <src> <dst-name>
	local src="$1" dst="$2"
	if [ -f "$src" ]; then
		cp -f "$src" "$DIST_DIR/$dst" && echo "  $(basename "$dst") <- $src"
	else
		FAILED=$((FAILED+1))
		echo "  MISSING: $src"
		printf 'missing %s\n' "$src" >> "$STATE/missing.txt"
	fi
}

copy_pair cnet/glinkd/glinkd            bin/glinkd
copy_pair cnet/gdeliveryd/gdeliveryd    bin/gdeliveryd
copy_pair cnet/gauthd/gauthd            bin/gauthd
copy_pair cnet/gfaction/gfactiond       bin/gfactiond
copy_pair cnet/gamedbd/gamedbd.wdb      bin/gamedbd
copy_pair cnet/uniquenamed/uniquenamed  bin/uniquenamed
copy_pair cnet/logservice/logservice    bin/logservice
copy_pair cgame/gs/gs                   bin/gs

copy_pair cnet/perf/libperf.a           lib/libperf.a
copy_pair cnet/io/libgsio.a             lib/libgsio.a
copy_pair cnet/gamed/libgsPro2.a        lib/libgsPro2.a
copy_pair cnet/gdbclient/libdbCli.a     lib/libdbCli.a
copy_pair cnet/logclient/liblogCli.a    lib/liblogCli.a
copy_pair cskill/skill/libskill.a       lib/libskill.a
copy_pair cgame/collision/libTrace.a    lib/libTrace.a
copy_pair cgame/libonline.a             lib/libonline.a
copy_pair cgame/libcommon.a             lib/libcommon.a

# ------------------------------------------------------------ manifest --------
{
	echo "idealworld-152-server build manifest"
	echo "===================================="
	echo "date        : $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
	echo "commit      : $(git rev-parse HEAD 2>/dev/null || echo unknown)"
	echo "branch      : $(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
	echo "host        : $(uname -srm)"
	echo "gcc         : $(gcc --version | head -n1)"
	echo "g++         : $(g++ --version | head -n1)"
	echo "libc        : $(ldd --version 2>/dev/null | head -n1)"
	echo "openssl     : $(pkg-config --modversion openssl 2>/dev/null || echo n/a)"
	echo "pcre        : $(pkg-config --modversion libpcre 2>/dev/null || echo n/a)"
	echo
	echo "build steps"
	echo "-----------"
	for line in "${STEP_RESULT[@]}"; do echo "  $line"; done
	echo
	echo "binaries"
	echo "--------"
	for f in "$DIST_DIR"/bin/*; do
		[ -f "$f" ] || continue
		printf '  %-14s %8s bytes  %s\n' "$(basename "$f")" \
			"$(stat -c%s "$f")" "$(md5sum "$f" | cut -c1-32)"
	done
	echo
	echo "static libraries"
	echo "----------------"
	for f in "$DIST_DIR"/lib/*; do
		[ -f "$f" ] || continue
		printf '  %-16s %8s bytes  %s\n' "$(basename "$f")" \
			"$(stat -c%s "$f")" "$(md5sum "$f" | cut -c1-32)"
	done
} > "$DIST_DIR/MANIFEST.txt"
cat "$DIST_DIR/MANIFEST.txt"

( cd "$DIST_DIR" && find bin lib -type f 2>/dev/null | sort | xargs -r sha256sum > SHA256SUMS.txt )

if [ "$FAILED" -ne 0 ]; then
	echo
	echo "=== BUILD INCOMPLETE: $FAILED failing step(s)/missing product(s) ==="
	[ -f "$STATE/missing.txt" ] && cat "$STATE/missing.txt"
	[ "$STRICT" = "1" ] && exit 1
fi

echo
echo "=== dist/ contents ==="
ls -lhR "$DIST_DIR" | head -n 60
echo "BUILD OK"
