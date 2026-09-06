#!/usr/bin/env bash
#
# setup-runtime.sh - assemble a runnable idealworld-152 server tree from
# (a) the binaries produced by ./tools/ci-build.sh or a GitHub release
#     tarball / unpacked release directory, and
# (b) the config files shipped in this repository,
# then patch the configs for a single-host (all on 127.0.0.1) install.
#
# The result is operated with tools/pwctl.sh (copied in as ./pwctl.sh):
#
#     tools/setup-runtime.sh ~/pw
#     # drop your PW-152 game data package into ~/pw/gs/data/
#     ~/pw/pwctl.sh            <- one-click start
#
# Usage:
#   setup-runtime.sh <runtime-dir> [binaries]
#
#     <runtime-dir>  where to build the tree (created; must be empty or
#                    nonexistent unless --force is given)
#     [binaries]     dist/ directory, a release tar.gz, or an unpacked
#                    release directory.  Default: <repo>/dist if present,
#                    else the newest idealworld-152-server-linux-*.tar.gz
#                    in the current directory.
#
#   --force         reuse an existing runtime dir: refresh binaries, configs
#                   and scripts, but never touch databases (dbhome/, dbhomewdb/,
#                   uname/), logs, or gs/data/
#
# Layout produced (see the header of pwctl.sh):
#   <runtime>/{gauthd,gdeliveryd,glinkd,gfaction,gamedbd,uniquenamed,logservice,gs}/
#   <runtime>/logs  <runtime>/pids  <runtime>/pwctl.sh  <runtime>/gs/worlds.list
#
set -u

FORCE=0
args=()
for a in "$@"; do
	case "$a" in
		--force) FORCE=1 ;;
		-h|--help) sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
		*) args+=("$a") ;;
	esac
done
RT="${args[0]:-}"; BINS="${args[1]:-}"
[ -n "$RT" ] || { sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//' >&2; echo "error: <runtime-dir> is required" >&2; exit 1; }

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # this repository

say()  { echo "[setup] $*"; }
die()  { echo "[setup] ERROR: $*" >&2; exit 1; }

# ------------------------------------------------------------- binaries -----
NEEDED="glinkd gdeliveryd gauthd gfactiond gamedbd uniquenamed logservice gs"

resolve_bins() {
	if [ -n "$BINS" ]; then
		[ -e "$BINS" ] || die "binaries location not found: $BINS"
		if [ -d "$BINS" ] && [ -x "$BINS/bin/gs" ]; then echo "$BINS/bin"; return 0; fi     # unpacked release dir
		if [ -d "$BINS" ] && [ -x "$BINS/gs" ]; then echo "$BINS"; return 0; fi             # dist/bin itself
		if [ -d "$BINS" ]; then echo "$BINS"; return 0; fi                                  # dir of bins
		case "$BINS" in
			*.tar.gz|*.tgz) mkdir -p "$RT/.unpacked" && tar -xzf "$BINS" -C "$RT/.unpacked" \
				|| die "cannot unpack $BINS"
				local d; d="$(find "$RT/.unpacked" -name gs -type f -printf '%h\n' | head -1)"
				[ -n "$d" ] || die "no binaries found inside $BINS"; echo "$d"; return 0 ;;
		esac
		die "unsupported binaries location: $BINS (want dist/, release dir, or release .tar.gz)"
	fi
	if [ -x "$SRC/dist/bin/gs" ]; then echo "$SRC/dist/bin"; return 0; fi
	local t; t="$(ls -t idealworld-152-server-linux-*.tar.gz "$SRC"/idealworld-152-server-linux-*.tar.gz 2>/dev/null | head -1)"
	if [ -n "$t" ]; then
		say "using release tarball $t"
		mkdir -p "$RT/.unpacked" && tar -xzf "$t" -C "$RT/.unpacked" || die "cannot unpack $t"
		local d; d="$(find "$RT/.unpacked" -name gs -type f -printf '%h\n' | head -1)"
		[ -n "$d" ] || die "no binaries found inside $t"; echo "$d"; return 0
	fi
	die "no binaries found. Run ./tools/ci-build.sh first (dist/), or pass a release tarball."
}

# ------------------------------------------------------------- runtime ------
if [ -d "$RT" ] && [ -n "$(ls -A "$RT" 2>/dev/null)" ] && [ "$FORCE" != 1 ]; then
	die "$RT exists and is not empty (use --force to refresh binaries/configs in place)"
fi
mkdir -p "$RT" || die "cannot create $RT"
RT="$(cd "$RT" && pwd)"
BINDIR="$(resolve_bins)" || exit 1
say "binaries: $BINDIR"
say "configs : $SRC"

for d in gauthd gdeliveryd glinkd gfaction gamedbd uniquenamed logservice gs; do
	mkdir -p "$RT/$d"
done
mkdir -p "$RT/logs" "$RT/pids"

for b in $NEEDED; do
	[ -x "$BINDIR/$b" ] || die "missing binary: $BINDIR/$b"
	cp -f "$BINDIR/$b" "$RT/placeholder_bin_$b" 2>/dev/null && rm -f "$RT/placeholder_bin_$b" # probe writability
done
# place binaries (gamedbd lives in gamedbd/, gfactiond in gfaction/, ...)
cp -f "$BINDIR/gauthd"       "$RT/gauthd/gauthd"
cp -f "$BINDIR/gdeliveryd"   "$RT/gdeliveryd/gdeliveryd"
cp -f "$BINDIR/glinkd"       "$RT/glinkd/glinkd"
cp -f "$BINDIR/gfactiond"    "$RT/gfaction/gfactiond"
cp -f "$BINDIR/gamedbd"      "$RT/gamedbd/gamedbd"
cp -f "$BINDIR/uniquenamed"  "$RT/uniquenamed/uniquenamed"
cp -f "$BINDIR/logservice"   "$RT/logservice/logservice"
cp -f "$BINDIR/gs"           "$RT/gs/gs"
chmod +x "$RT"/*/* 2>/dev/null
say "binaries installed (8 daemons)"

# -------------------------------------------------------------- configs -----
# originals kept for reference in conf-origins/; working copies are patched
mkdir -p "$RT/conf-origins"
copy_conf() { # <repo-file> <runtime-file>
	[ -f "$1" ] || die "missing config in repo: $1"
	cp -f "$1" "$RT/conf-origins/$(basename "$2").orig"
	cp -f "$1" "$RT/$2"
}

copy_conf "$SRC/cnet/gauthd/gauthd.conf"            gauthd/gauthd.conf
copy_conf "$SRC/cnet/gdeliveryd/gamesys.conf"       gdeliveryd/gamesys.conf
copy_conf "$SRC/cnet/glinkd/gamesys.conf"           glinkd/gamesys.conf
copy_conf "$SRC/cnet/gfaction/gamesys.conf"         gfaction/gamesys.conf
copy_conf "$SRC/cnet/gamedbd/gamesys.conf"          gamedbd/gamesys.conf
copy_conf "$SRC/cnet/uniquenamed/uniquenamed.conf"  uniquenamed/uniquenamed.conf
copy_conf "$SRC/cnet/logservice/logservice.conf"    logservice/logservice.conf
copy_conf "$SRC/cgame/gs/gs.conf"                   gs/gs.conf
copy_conf "$SRC/cgame/gs/gs2.conf"                  gs/gs2.conf
copy_conf "$SRC/cgame/gs/gmserver.conf"             gs/gmserver.conf
copy_conf "$SRC/cgame/gs/gmserver2.conf"            gs/gmserver2.conf
copy_conf "$SRC/cgame/gs/gsalias.conf"              gs/gsalias.conf
copy_conf "$SRC/cgame/gs/ptemplate.conf"            gs/ptemplate.conf
copy_conf "$SRC/cgame/gs/rare_item.conf"            gs/rare_item.conf
[ -f "$SRC/cgame/gs/gmserver3.conf" ] && copy_conf "$SRC/cgame/gs/gmserver3.conf" gs/gmserver3.conf
say "configs installed (originals in conf-origins/)"

# gdeliveryd: [GAuthClient] defaults to certificate auth (au_cert=true)
# against an "AU" billing authority that does not exist in a private setup -
# announce the zone directly instead.
if ! grep -q "^au_cert" "$RT/gdeliveryd/gamesys.conf"; then
	sed -i '/^\[GAuthClient\]/a au_cert\t\t\t=\tfalse' "$RT/gdeliveryd/gamesys.conf"
	say "  gdeliveryd/gamesys.conf   au_cert=false (no AU certificate authority)"
fi

# ------------------------------------------------------- config patching ----
PATCHED=0
patch_all_addresses() { # every conf in the tree: point client sections at localhost
	local f
	for f in "$RT"/*/*.conf; do
		awk -v file="$f" '
			/^[ \t]*address[ \t]*=[ \t]*/ {
				v = $0
				sub(/^[ \t]*address[ \t]*=[ \t]*/, "", v)
				if (v ~ /^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$/ && v != "0.0.0.0" && v != "127.0.0.1") {
					printf "  %-28s address %s -> 127.0.0.1\n", FILENAME, v > "/dev/stderr"
					sub(/=[ \t]*[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+[ \t]*$/, "= 127.0.0.1")
				}
			}
			{ print }' "$f" > "$f.tmp" && mv "$f.tmp" "$f" || rm -f "$f.tmp"
	done
}

say "patching configs for single-host (127.0.0.1):"
patch_all_addresses

# gauthd: align its listen port with gdeliveryd/gamesys.conf GAuthClient (29200)
sed -i -E 's/^([ \t]*port[ \t]*=[ \t]*)9200[ \t]*$/\129200/' "$RT/gauthd/gauthd.conf" \
	&& say "  gauthd/gauthd.conf        port 9200 -> 29200 (match GAuthClient)"
# gauthd <-> gdeliveryd transport security: gdeliveryd's [GAuthClient] uses
# ARC4 (isec/osec = 2) with string keys; gauthd's shipped iseckey/oseckey do
# not match them, so gauthd decrypts garbage ("Protocol Unknown"), and drops
# the session in a reconnect loop.  Mirror the client keys into gauthd
# (server iseckey = client oseckey and vice versa).
GISEC="$(awk '/^\[GAuthClient\]/{f=1;next} /^\[/{f=0} f && $1=="iseckey"{print $3; exit}' "$RT/gdeliveryd/gamesys.conf")"
GOSEC="$(awk '/^\[GAuthClient\]/{f=1;next} /^\[/{f=0} f && $1=="oseckey"{print $3; exit}' "$RT/gdeliveryd/gamesys.conf")"
if [ -n "$GISEC" ] && [ -n "$GOSEC" ]; then
	awk -v ik="$GOSEC" -v ok="$GISEC" '
		/^\[/ { insec = ($0 ~ /^\[GAuthServer\]/) }
		insec && $1 == "isec"  { print "isec\t\t\t=\t2"; next }
		insec && $1 == "osec"  { print "osec\t\t\t=\t2"; next }
		insec && $1 == "iseckey" { print "iseckey\t\t\t=\t" ik; next }
		insec && $1 == "oseckey" { print "oseckey\t\t\t=\t" ok; next }
		{ print }' "$RT/gauthd/gauthd.conf" > "$RT/gauthd/gauthd.conf.tmp" && mv "$RT/gauthd/gauthd.conf.tmp" "$RT/gauthd/gauthd.conf"
	say "  gauthd/gauthd.conf        ARC4 keys mirrored from gdeliveryd GAuthClient"
else
	say "  WARNING: no iseckey/oseckey found in gdeliveryd GAuthClient - left gauthd keys as shipped"
fi

# gamedbd: single zone - keep zoneid consistent with gdeliveryd (zoneid=1)
sed -i -E 's/^([ \t]*zoneid[ \t]*=[ \t]*)4[ \t]*$/\11/' "$RT/gamedbd/gamesys.conf" \
	&& say "  gamedbd/gamesys.conf      zoneid 4 -> 1 (match gdeliveryd)"

# WDB storage: GetStorage() returns NULL (-> crash on first use) for any table
# not listed in [storagewdb] tables= - the shipped lists are missing several
# tables the binaries actually use.  Append the missing ones.
sed -i 's/^\(tables[ \t]*=[ \t]*.*\)$/\1,conv_temp,crslogicuid,factionfortress,factionrelation,force,friendext,globalcontrol,playershop,recalluser,rolenamehis,syslog,uniquedata,userstore,webtrade,webtradesold,kingelection,mappassword,playerprofile,serverdata,weborderitem/' \
	"$RT/gamedbd/gamesys.conf" \
	&& say "  gamedbd/gamesys.conf      storagewdb tables += 20 missing tables"
sed -i 's/^\(tables[ \t]*=[ \t]*.*\)$/\1,logicuid,unamefamily/' "$RT/uniquenamed/uniquenamed.conf" \
	&& say "  uniquenamed/*.conf        storagewdb tables += logicuid,unamefamily"

# logservice: /export/logs -> ./logdata
sed -i 's|/export/logs/|./logdata/|g' "$RT/logservice/logservice.conf" \
	&& say "  logservice/logservice.conf /export/logs -> ./logdata"

# uniquenamed: legacy [storage] /export/uname -> ./uname (wdb section already relative)
sed -i 's|/export/unamebackup|./unamebackup|g; s|/export/uname|./uname|g' "$RT/uniquenamed/uniquenamed.conf" \
	&& say "  uniquenamed/*.conf        /export/uname -> ./uname"

# gs: data paths /home/cui/nn/... -> ./data/... ; map alias GTEST -> localhost
sed -i 's|/home/cui/nn/|./data/|g' "$RT/gs/gs.conf" \
	&& say "  gs/gs.conf                /home/cui/nn -> ./data"
sed -i 's/\(WORLD[0-9]*[ \t]*=[ \t]*\)GTEST/\1localhost/; s/\(ARENA[0-9]*[ \t]*=[ \t]*\)GTEST/\1localhost/; s/\(INSTANCE[0-9]*[ \t]*=[ \t]*\)GTEST/\1localhost/' "$RT/gs/gs.conf" \
	&& say "  gs/gs.conf                AddrAlias GTEST -> localhost"

# gdeliveryd and gfactiond load a sensitive-word filter (Matcher::Load) from
# ./filters in their working directory and refuse to start without it.  An
# empty file is explicitly supported ("checking disabled"); fill it with a
# GBK/UCS2 wordlist to actually filter chat/names.
for d in gdeliveryd gfaction; do
	[ -f "$RT/$d/filters" ] || : > "$RT/$d/filters"
done
say "empty word-filter files created (gdeliveryd/filters, gfaction/filters)"

# gdeliveryd reads several data tables from its working directory at startup
# and exits fatally when they are missing.  auctionid.txt ships in the repo
# (GBK item->category table); the others get empty stubs - replace them with
# the real ones from your data package when you have it.
cp -f "$SRC/cnet/gdeliveryd/auctionid.txt" "$RT/gdeliveryd/auctionid.txt"
[ -f "$RT/gdeliveryd/webtradeid.txt"     ] || : > "$RT/gdeliveryd/webtradeid.txt"
[ -f "$RT/gdeliveryd/sysauctionlist.txt" ] || : > "$RT/gdeliveryd/sysauctionlist.txt"
# domain.sev / domain2.sev (territory & country battle maps): empty = no
# battle domains.  domain.sev: <int count>[entries]<int battletime count>
# [entries]<int battletime_max>;  domain2.sev: <uint timestamp><int count>
if [ ! -f "$RT/gdeliveryd/domain.sev" ]; then
	printf '\0\0\0\0\0\0\0\0\0\0\0\0' > "$RT/gdeliveryd/domain.sev"
fi
if [ ! -f "$RT/gdeliveryd/domain2.sev" ]; then
	printf '\0\0\0\0\0\0\0\0' > "$RT/gdeliveryd/domain2.sev"
fi
say "gdeliveryd data files staged (auctionid.txt from repo, empty battle-domain stubs)"

# gamedbd refuses to start without serverlist.sev in its working directory
# (GameDBManager::LoadZoneNameConfig: <int32 zoneid><size_t namelen><name in
# UTF-16LE, namelen*2 bytes>).  Synthesize a single-zone file for zoneid=1,
# matching gdeliveryd's zoneid - replace it with your real zone list if you
# have one (note: original 32-bit builds wrote a 4-byte namelen, this file is
# for the 64-bit build's 8-byte size_t).
if [ ! -f "$RT/gamedbd/serverlist.sev" ]; then
	if python3 - "$RT/gamedbd/serverlist.sev" <<'PYEOF'
import struct, sys
name = "zone1"
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<i", 1))              # zoneid  (int)
    f.write(struct.pack("<Q", len(name)))      # namelen (size_t, 8 bytes on x86_64)
    f.write(name.encode("utf-16-le"))          # name, namelen*2 bytes
PYEOF
	then say "gamedbd/serverlist.sev synthesized (single zone 'zone1', zoneid=1)"
	else say "WARNING: could not run python3 - create gamedbd/serverlist.sev manually (RUNBOOK.md)"
	fi
fi

# ------------------------------------------------------ storage & data ------
for d in gauthd glinkd gfaction gamedbd; do
	mkdir -p "$RT/$d/dbhome/dbdata" "$RT/$d/dbhome/dblogs" "$RT/$d/backup"
done
mkdir -p "$RT/gamedbd/dbhomewdb/dbdata" "$RT/gamedbd/dbhomewdb/dblogs" "$RT/gamedbd/backupwdb"
mkdir -p "$RT/uniquenamed/uname/dbdata" "$RT/uniquenamed/uname/dblogs" "$RT/uniquenamed/unamebackup"
mkdir -p "$RT/logservice/logdata"
say "storage directories created (dbhome/ dbhomewdb/ uname/ logdata/ + backups)"

# gs data skeleton: [Template] files + per-map base_path dirs + MoveMap dirs
mkdir -p "$RT/gs/data" "$RT/gs/movemap" "$RT/gs/watermap" "$RT/gs/airmap"
for m in world b01 a01 a02 a05 a06 a07; do mkdir -p "$RT/gs/data/$m/map"; done
if [ ! -f "$RT/gs/data/README.txt" ]; then
	cat > "$RT/gs/data/README.txt" <<'EOF'
Drop a PW-152 era data package here. gs.conf (patched) expects:

  elements.data, tasks.data, dyn_tasks.data, world_targets.sev, aipolicy.data
  world/  b01/  a01/  a02/  a05/  a06/  a07/   (per-map dirs, each with map/
  subdir, npcgen.data, precinct.sev, region.sev, path.sev)
  ../movemap/  ../watermap/  ../airmap/        (collision/movement maps)

Until these exist, pwctl.sh will start every daemon except gs.
EOF
	say "gs/data skeleton created (fill it - see gs/data/README.txt)"
fi

# gs.conf references RestartShell=restart_zhang, executed if a world crashes;
# install a stub that logs the event instead of failing to exec a missing file
if [ ! -f "$RT/gs/restart_zhang" ]; then
	cat > "$RT/gs/restart_zhang" <<'EOF'
#!/bin/sh
# restart hook referenced by gs.conf (RestartShell). Original name kept.
echo "$(date '+%F %T') gs asked for restart (args: $*)" >> ../logs/restart_zhang.log
EOF
	chmod +x "$RT/gs/restart_zhang"
	say "gs/restart_zhang restart hook stub installed"
fi

# which worlds to launch: first world of gs.conf [General] world_servers
if [ ! -f "$RT/gs/worlds.list" ]; then
	w="$(sed -n 's/^[ \t]*world_servers[ \t]*=[ \t]*//p' "$RT/gs/gs.conf" | head -1 | cut -d';' -f1 | tr -d ' \t')"
	echo "${w:-gs01}" > "$RT/gs/worlds.list"
fi
say "worlds.list: $(tr '\n' ' ' < "$RT/gs/worlds.list")"

# -------------------------------------------------------------- pwctl -------
cp -f "$SRC/tools/pwctl.sh" "$RT/pwctl.sh" && chmod +x "$RT/pwctl.sh" || die "cannot install pwctl.sh"
rm -rf "$RT/.unpacked"

# -------------------------------------------------------------- summary -----
echo
say "runtime tree ready:"
( cd "$RT" && find . -maxdepth 2 \( -name dbdata -o -name dblogs \) -prune -o -print | sort | sed 's/^/  /' | head -60 )
cat <<EOF

next steps:
  1. drop the game data package into $RT/gs/data/   (skip if already there)
  2. start everything:   cd $RT && ./pwctl.sh
  3. watch it come up:   ./pwctl.sh status ; ./pwctl.sh logs gdeliveryd
  4. clients connect to  glinkd  (port 9001, version 804 in the conf)
EOF
