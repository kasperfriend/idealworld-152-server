#!/usr/bin/env bash
#
# Build the idealworld-152 server as native Windows binaries (PE32+) and
# stage the results in dist-win/.
#
# Two toolchain modes are supported:
#
#   native (default, used by .github/workflows/windows.yml on windows-latest):
#     WP_CC / WP_CXX / WP_AR from the MSYS2 mingw64 environment (gcc).
#     Real pcre and openssl import libraries are linked for the daemons that
#     need them.
#
#   zig (fast local iteration on Linux):
#     ZIG=<path-to-zig> compiles everything with a clang based
#     x86_64-windows-gnu cross toolchain.  pcre/openssl are replaced by
#     compile/link stubs (win32/zigonly, win32/wpcre.cpp) so every object and
#     every link can still be exercised locally.
#
# Products (dist-win/bin): glinkd.exe, gdeliveryd.exe, gauthd.exe,
#                          gfactiond.exe, gamedbd.exe, uniquenamed.exe,
#                          logservice.exe, gs.exe
#
# Environment:
#   JOBS         -j value (default: nproc)
#   DIST_DIR     where to stage the result (default: <repo>/dist-win)
#   STRICT       0 to keep going after a failing step (default: 1)
#   ZIG          zig binary (enables the zig mode)
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
DIST_DIR="${DIST_DIR:-$ROOT/dist-win}"
STATE="$ROOT/ci-build-win"
TC="$STATE/tc"
STRICT="${STRICT:-1}"

rm -rf "$STATE" "$DIST_DIR"
mkdir -p "$STATE" "$TC" "$DIST_DIR/bin" "$DIST_DIR/lib"

# Like the Linux driver: never reuse objects/archives from a previous (or
# Linux) build; every step below regenerates what it needs.
echo "=== removing stale build artifacts ==="
find cnet cgame cskill -name '*.o' -delete 2>/dev/null
find cnet cgame cskill -name '*.a' -delete 2>/dev/null

declare -a STEP_RESULT
FAILED=0

step() { # step <name> <command...>
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
		echo "FAILED (exit $rc) - see $STATE/$name.log"
		grep -E "error:|Error [0-9]+|undefined reference|undefined symbol|multiple definition|cannot find|No such file|fatal error" \
			"$STATE/$name.log" | head -n 80
		tail -n 15 "$STATE/$name.log"
	fi
	echo "::endgroup::"
	STEP_RESULT+=("$name: $(cat "$STATE/$name.status")")
	return 0
}

# ---------------------------------------------------------------- toolchain
echo "=== environment ==="
uname -a

MODE="native"
WZ_ONLY=0
if [ -n "${ZIG:-}" ]; then
	MODE="zig"
	WZ_ONLY=1
	echo "toolchain: zig ($ZIG)"
	$ZIG version 2>/dev/null | head -n1 || true

	cat > "$TC/cc" <<EOF
#!/usr/bin/env bash
exec "$ZIG" cc -target x86_64-windows-gnu -D__MINGW_FORTIFY_LEVEL=0 "\$@"
EOF
	cat > "$TC/cxx" <<EOF
#!/usr/bin/env bash
exec "$ZIG" c++ -target x86_64-windows-gnu -D__MINGW_FORTIFY_LEVEL=0 "\$@"
EOF
	cat > "$TC/ld" <<EOF
#!/usr/bin/env bash
exec "$ZIG" c++ -target x86_64-windows-gnu -D__MINGW_FORTIFY_LEVEL=0 \
	"\$@" -lws2_32 -lwsock32 -lwinpthread -lbcrypt -lpsapi \
	"$STATE/winposix.a" "$STATE/winiconv.o" "$STATE/wsyslog.o" \
	"$STATE/wrusage.o" "$STATE/wpmd5.o" "$STATE/wpcre.o" "$STATE/wpopenssl.o"
EOF
	chmod +x "$TC/cc" "$TC/cxx" "$TC/ld"
	# cskill's Makefilelib adds -finput-charset=ISO-8859-1/-fexec-charset=...
	# which clang does not accept; strip them (gcc-only flags).
	cat > "$TC/cxx_ncs" <<EOF
#!/usr/bin/env bash
args=()
while [ \$# -gt 0 ]; do
	case "\$1" in
		-finput-charset=*|-fexec-charset=*) shift ;;
		*) args+=("\$1"); shift ;;
	esac
done
exec "$TC/cxx" "\${args[@]}"
EOF
	chmod +x "$TC/cxx_ncs"
	WP_CC="$TC/cxx"; WP_CXX="$TC/cxx"; WP_LD="$TC/ld"
	CSKCC="$TC/cxx_ncs"
	WP_AR="ar"
	WP_CCBIN="$TC/cc"
	# avoid linking pcre/openssl when they are not available
	PCRELIB=""
	CRYPTOLIB=""
	DLLIB=""
else
	WP_CC="${WP_CC:-gcc}"; WP_CXX="${WP_CXX:-g++}"; WP_AR="${WP_AR:-ar}"
	echo "toolchain: native ($WP_CC / $WP_CXX)"
	# Native link wrapper: same role as the zig $TC/ld above, minus the
	# zig-only OpenSSL stubs (native links the real -lcrypto).
	cat > "$TC/ld" <<EOF
#!/usr/bin/env bash
exec "$WP_CXX" "\$@" -lws2_32 -lwsock32 -lwinpthread -lbcrypt -lpsapi \
	"$STATE/winposix.a" "$STATE/winiconv.o" "$STATE/wsyslog.o" \
	"$STATE/wrusage.o" "$STATE/wpmd5.o" "$STATE/wpcre.o"
EOF
	chmod +x "$TC/ld"
	WP_LD="$TC/ld"
	CSKCC="$WP_CXX"
	WP_CCBIN="$WP_CC"
	PCRELIB="-lpcre"
	CRYPTOLIB="-lcrypto"
	DLLIB=""
fi

# ---------------------------------------------------------------- flags
P1="$ROOT/win32/posix"     # Windows POSIX shim headers (always first)
P2=""
if [ "$MODE" = "zig" ]; then
	P2="$ROOT/win32/zigonly"   # clang-only shims (ext/hash_map, pcre/openssl stubs)
fi

NETINC=" -I$P1 ${P2:+-I$P2} -I$ROOT/cnet -I$ROOT/cnet/inc -I$ROOT/cnet/include \
 -I$ROOT/cnet/common -I$ROOT/cnet/io -I$ROOT/cnet/perf -I$ROOT/cnet/rpc \
 -I$ROOT/cnet/inl -I$ROOT/cnet/rpcdata -I$ROOT/cnet/logclient \
 -I$ROOT/cnet/storage -I$ROOT/cnet/gamed -I$ROOT/cnet/gamed/header \
 -I$ROOT/cnet/gamed/header/include -I$ROOT/cnet/gamedbd -I$ROOT/cnet/gdeliveryd \
 -I$ROOT/cnet/gfaction -I$ROOT/cnet/gfaction/operations -I$ROOT/cnet/gauthd \
 -I$ROOT/cnet/glinkd -I$ROOT/cnet/uniquenamed -I$ROOT/cnet/logservice \
 -I$ROOT/cnet/log_inl \
 -I$ROOT/cnet/gdbclient -I$ROOT/cskill -I$ROOT/cskill/header/include"

GAMEINC=" -I$P1 ${P2:+-I$P2} -I$ROOT/cgame/include -I$ROOT/cgame \
 -I$ROOT/cgame/common -I$ROOT/cgame/io -I$ROOT/cgame/collision \
 -I$ROOT/cgame/gs -I$ROOT/cgame/gs/instance -I$ROOT/cgame/gs/io \
 -I$ROOT/cgame/gs/item -I$ROOT/cgame/gs/mobile -I$ROOT/cgame/gs/pathfinding \
 -I$ROOT/cgame/gs/task -I$ROOT/cgame/gs/template -I$ROOT/cgame/gs/ai \
 -I$ROOT/cgame/gs/wallow -I$ROOT/cgame/libgs -I$ROOT/cnet/inc -I$ROOT/cnet \
 -I$ROOT/cskill -I$ROOT/cskill/header/include"

# Linux Makefiles append these with `+=`, which command-line overrides
# discard, so the driver re-applies them.
D_NET="-DWIN32 -D_REENTRANT_ -D_GNU_SOURCE -D__MINGW_FORTIFY_LEVEL=0"
D_GAME="-DWIN32 -D_DEBUG -D__THREAD_SPIN_LOCK__ -D__MINGW_FORTIFY_LEVEL=0"
D_LOGC="-DUSE_LOGCLIENT"
D_WDB="-DUSE_WDB -DMPPC_4WAY -DUSE_TRANSACTION -D_FILE_OFFSET_BITS=64"

CFLAGS="-std=gnu++14 -fpermissive -w -O0 -fno-omit-frame-pointer -include winposix.h -include cstring -include cstdio -include cstdlib -include cstdint -include climits -include ctime"

# cnet daemons are built through their own Makefiles with fully overridden
# DEFINES / INCLUDES / LDFLAGS.
build_net_daemon() { # <stepname> <dir> <target> <extra defs>
	local name="$1" dir="$2" target="$3" defs="$4"
	step "$name" make -C "$ROOT/$dir" \
		CC="$WP_CC" CPP="$WP_CXX" LD="$WP_LD" AR="$WP_AR" \
		DEFINES="$D_NET $CFLAGS $defs" \
		INCLUDES="$NETINC" LDFLAGS="-O0" CFLAGS="$CFLAGS" \
		PCRELIB="$PCRELIB" CRYPTOLIB="$CRYPTOLIB" DLLIB="$DLLIB" \
		"$target" -k -j"$JOBS"
}

copy_pair() { # <src> <dst>
	local src="$1" dst="$2"
	if [ -f "$src" ]; then
		cp -f "$src" "$DIST_DIR/$dst" && echo "  $(basename "$dst") <- $src"
	else
		FAILED=$((FAILED+1))
		echo "  MISSING: $src"
		printf 'missing %s\n' "$src" >> "$STATE/missing.txt"
	fi
}

# ------------------------------------------------------------------ winposix
echo "=== building win32 shim layer ==="
step "winposix" bash -c "cd '$ROOT' && $WP_CXX $D_NET $CFLAGS -I$P1 ${P2:+-I$P2} -c win32/winposix.cpp -o '$STATE/winposix.o' && $WP_AR crs '$STATE/winposix.a' '$STATE/winposix.o'"
step "winiconv" bash -c "cd '$ROOT' && $WP_CXX $D_NET $CFLAGS -I$P1 ${P2:+-I$P2} -c win32/winiconv.cpp -o '$STATE/winiconv.o'"
step "wsyslog" bash -c "cd '$ROOT' && $WP_CXX $D_NET $CFLAGS -I$P1 ${P2:+-I$P2} -c win32/wsyslog.cpp -o '$STATE/wsyslog.o'"
step "wrusage" bash -c "cd '$ROOT' && $WP_CXX $D_NET $CFLAGS -I$P1 ${P2:+-I$P2} -c win32/wrusage.cpp -o '$STATE/wrusage.o'"
if [ "$MODE" = "zig" ]; then
	step "wpmd5" bash -c "cd '$ROOT' && $WP_CXX $D_NET $CFLAGS -I$P1 -I$P2 -c win32/wpmd5.cpp -o '$STATE/wpmd5.o'"
	step "wpcre" bash -c "cd '$ROOT' && $WP_CXX $D_NET $CFLAGS -I$P1 -I$P2 -c win32/wpcre.cpp -o '$STATE/wpcre.o'"
	step "wpopenssl" bash -c "cd '$ROOT' && $WP_CXX $D_NET $CFLAGS -I$P1 -I$P2 -c win32/wpopenssl.cpp -o '$STATE/wpopenssl.o'"
fi

# -------------------------------------------------------------------- perf
# The x86_64/*.s files are GAS/Linux-only, so Windows builds perf from
# portable C: win32/wperf.c provides the live symbols (crc32 and the base64
# pair) while the remaining members are empty placeholders that keep the
# link layout identical to Linux.
step "cnet-perf" bash -c "
	cd '$ROOT' || exit 1
	mkdir -p cnet/perf/x86_64
	$WP_CCBIN -O2 -w -c win32/wperf.c -o cnet/perf/x86_64/wperf.o || exit 1
	for f in md5 sha1 rc4 mppc256 aes bf crc32 base64; do
		echo \"int wp_perf_empty_\$f = 0;\" > '$STATE/empty_'\$f.c
		$WP_CCBIN -O2 -w -c '$STATE/empty_'\$f.c -o cnet/perf/x86_64/\$f.o || exit 1
	done
	(cd cnet/perf/x86_64 && $WP_AR crs libperf.a wperf.o md5.o sha1.o rc4.o mppc256.o aes.o bf.o crc32.o base64.o) || exit 1
"
cp -f "$ROOT/cnet/perf/x86_64/libperf.a" "$ROOT/cnet/perf/libperf.a" 2>/dev/null || true
copy_pair cnet/perf/libperf.a lib/libperf.a


# -------------------------------------------------------------- cnet libs --
# (needed by gs: libgsPro2 / libgsio / libdbCli / liblogCli / libskill)
cnet_lib() { # <stepname> <dir> <makefile> <target> <extra defs> <extra includes>
	local name="$1" dir="$2" mf="$3" tgt="$4" defs="$5" extrainc="$6"
	local mfarg=""
	[ -n "$mf" ] && mfarg="-f $mf"
	step "$name" make -C "$ROOT/$dir" $mfarg \
		CC="$WP_CC" CPP="$WP_CXX" LD="$WP_LD" AR="$WP_AR" \
		DEFINES="$D_NET $CFLAGS $defs" INCLUDES="$NETINC $extrainc" \
		LDFLAGS="-O0" CFLAGS="$CFLAGS" "$tgt" -k -j"$JOBS"
}
cnet_lib "cnet-io-lib"     cnet/io      ""       lib ""  ""
cnet_lib "cnet-gamed-lib"  cnet/gamed   ""       lib "-D__USE_SPEC_GAMEDATASEND__" "-I$ROOT/cnet/gamed/header -I$ROOT/cnet/gamed/header/include/common"
cnet_lib "cnet-gdbclient"  cnet/gdbclient ""     lib ""  ""
cnet_lib "cnet-logclient"  cnet/logclient Makefile.gs lib "-DUSE_LOGCLIENT" ""
CSKINC=" -I$P1 ${P2:+-I$P2} -I$ROOT/cskill -I$ROOT/cskill/expr \
 -I$ROOT/cskill/header -I$ROOT/cskill/header/include -I$ROOT/cskill/skill \
 -I$ROOT/cskill/skills -I$ROOT/cskill/simulator -I$ROOT/cskill/gen/src"
step "cskill-lib" make -C "$ROOT/cskill/skill" -f ../Makefilelib \
	CC="$CSKCC" CPP="$CSKCC" LD="$WP_LD" AR="$WP_AR" \
	DEFINES="-DWIN32 -D_REENTRANT_ -D_GNU_SOURCE $CFLAGS -D_SKILL_SERVER" \
	INCLUDES="$CSKINC" LDFLAGS="-O0" CFLAGS="$CFLAGS" lib -k -j"$JOBS"


# ---------------------------------------------------------- cnet: daemons --
# netdefs: <extra-defines> <extra-libs>
build_net_daemon "logservice"  cnet/logservice logservice        ""
build_net_daemon "glinkd"      cnet/glinkd glinkd                "$D_LOGC"
build_net_daemon "gauthd"      cnet/gauthd gauthd                ""
build_net_daemon "uniquenamed" cnet/uniquenamed uniquenamed      "$D_WDB $D_LOGC"
build_net_daemon "gfaction"    cnet/gfaction gfactiond           "$D_LOGC"
build_net_daemon "gdeliveryd"  cnet/gdeliveryd gdeliveryd        "$D_LOGC"
build_net_daemon "gamedbd"     cnet/gamedbd gamedbd.wdb          "-DUSE_DB $D_WDB $D_LOGC"

# ------------------------------------------------------------- cgame: gs --
# cgame common/collision/libs/gs build through their own Makefiles with the
# Rules.make variable set overridden from the command line.
# NOTE: CC/CPP re-apply D_GAME+CFLAGS because Rules.make bakes all of its
# defines into CC/CPP, which these overrides replace (bare CC would compile
# with no -DWIN32 and no winposix preinclude). AR needs its 'crs' operation
# for the same reason.
step "cgame-common" make -C "$ROOT/cgame/common" \
	CC="$WP_CC $D_GAME $CFLAGS" CPP="$WP_CXX $D_GAME $CFLAGS" LD="$WP_LD" AR="$WP_AR crs" \
	INC="$GAMEINC" -k -j"$JOBS"
step "cgame-collision" make -C "$ROOT/cgame/collision" \
	CC="$WP_CC $D_GAME $CFLAGS" CPP="$WP_CXX $D_GAME $CFLAGS" LD="$WP_LD" AR="$WP_AR crs" \
	INC="$GAMEINC" -k -j"$JOBS"
step "cgame-libs" make -C "$ROOT/cgame" lib \
	CC="$WP_CC $D_GAME $CFLAGS" CPP="$WP_CXX $D_GAME $CFLAGS" LD="$WP_LD" AR="$WP_AR crs" \
	INC="$GAMEINC" -k -j"$JOBS"
step "gs" make -C "$ROOT/cgame/gs" gs \
	CC="$WP_CC $D_GAME $CFLAGS" CPP="$WP_CXX $D_GAME $CFLAGS" LD="$WP_LD" AR="$WP_AR crs" \
	INC="$GAMEINC" CMLIB="$ROOT/cgame/libcommon.a $ROOT/cgame/libonline.a \
	$ROOT/cgame/libgs/gs/*.o $ROOT/cgame/libgs/io/*.o $ROOT/cgame/libgs/db/*.o \
	$ROOT/cskill/skill/*.o $ROOT/cskill/skills/*.o $ROOT/cgame/libgs/log/*.o \
	$ROOT/cgame/collision/libTrace.a" ALLLIB="-lws2_32 -lwinpthread -lbcrypt $PCRELIB $CRYPTOLIB" \
	-k -j"$JOBS"

# ------------------------------------------------------------- staging ------
echo "=== staging dist-win/ ==="
copy_pair cnet/glinkd/glinkd            bin/glinkd.exe
copy_pair cnet/gdeliveryd/gdeliveryd    bin/gdeliveryd.exe
copy_pair cnet/gauthd/gauthd            bin/gauthd.exe
copy_pair cnet/gfaction/gfactiond       bin/gfactiond.exe
copy_pair cnet/gamedbd/gamedbd.wdb      bin/gamedbd.exe
copy_pair cnet/uniquenamed/uniquenamed  bin/uniquenamed.exe
copy_pair cnet/logservice/logservice    bin/logservice.exe
copy_pair cgame/gs/gs                   bin/gs.exe

{
	echo "idealworld-152-server windows build manifest"
	echo "============================================"
	echo "date        : $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
	echo "commit      : $(git rev-parse HEAD 2>/dev/null || echo unknown)"
	echo "branch      : $(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
	echo "host        : $(uname -srm)"
	echo
	echo "build steps"
	echo "-----------"
	for line in "${STEP_RESULT[@]}"; do echo "  $line"; done
	echo
	echo "binaries"
	echo "--------"
	for f in "$DIST_DIR"/bin/*; do
		[ -f "$f" ] || continue
		printf '  %-16s %8s bytes\n' "$(basename "$f")" "$(stat -c%s "$f" 2>/dev/null || stat -f%z "$f")"
	done
} > "$DIST_DIR/MANIFEST.txt"
cat "$DIST_DIR/MANIFEST.txt"

if [ "$FAILED" -ne 0 ]; then
	echo
	echo "=== BUILD INCOMPLETE: $FAILED failing step(s) ==="
	[ -f "$STATE/missing.txt" ] && cat "$STATE/missing.txt"
	[ "$STRICT" = "1" ] && exit 1
fi

echo
echo "=== dist-win/ contents ==="
ls -lhR "$DIST_DIR" | head -n 40
echo "BUILD OK"
