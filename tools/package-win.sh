#!/usr/bin/env bash
#
# package-win.sh - build the "unpack and double-click" Windows server zip.
#
#   tools/package-win.sh [binaries] [output.zip]
#
#     binaries   dist-win/ directory (default), an unpacked release
#                directory, or a release .tar.gz - anything
#                tools/setup-runtime.sh accepts.
#     output.zip default: idealworld-152-server-windows-<date>.zip in the
#                current directory.
#
# The zip contains one folder with:
#
#   START-ALL.BAT START-GS.BAT STOP-ALL.BAT STATUS.BAT   (from tools/winpkg)
#   HOWTO-PLAY.TXT      short guide: what to add, what to run, how to log in
#   README-FIRST.TXT    the longer player-facing guide
#   RUNBOOK.MD          the technical runbook
#   <daemon>/<daemon>.exe + patched config + dbhome/      (tools/setup-runtime.sh)
#   *.dll            the mingw runtime libraries the .exe files import
#
# The game data (elements.data, maps, ...) and the game client are NOT
# packaged - they are not part of this repository.  HOWTO-PLAY.TXT says so
# and explains where they go.
#
# Environment:
#   DLL_PATH   extra directory to search for the runtime DLLs
#   PKG_NAME   name of the folder inside the zip
#
set -u

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINS="${1:-$SRC/dist-win}"
OUT="${2:-idealworld-152-server-windows-$(date +%Y%m%d).zip}"
NAME="${PKG_NAME:-idealworld-152-server-windows}"

say() { echo "[pkg] $*"; }
die() { echo "[pkg] ERROR: $*" >&2; exit 1; }

[ -e "$BINS" ] || die "binaries not found: $BINS (run ./tools/ci-build-win.sh first)"

STAGE="$(mktemp -d)" || die "cannot create a temporary directory"
trap 'rm -rf "$STAGE"' EXIT
RT="$STAGE/$NAME"

# ---------------------------------------------------------------- tree ------
say "assembling the runtime tree from $BINS"
"$SRC/tools/setup-runtime.sh" "$RT" "$BINS" >"$STAGE/setup.log" 2>&1 \
	|| { tail -n 25 "$STAGE/setup.log" >&2; die "setup-runtime.sh failed"; }
grep -E '^\[setup\]' "$STAGE/setup.log" | tail -n 6 | sed 's/^/[pkg]   /'

# --------------------------------------------------------------- launchers --
to_crlf() { sed -e 's/\r$//' -e 's/$/\r/' "$1" > "$2"; }   # batch files need CRLF

say "installing the launcher scripts and the guides"
for f in start-all.bat start-gs.bat stop-all.bat status.bat; do
	[ -f "$SRC/tools/winpkg/$f" ] || die "missing tools/winpkg/$f"
	to_crlf "$SRC/tools/winpkg/$f" "$RT/$(echo "$f" | tr 'a-z' 'A-Z')"
	chmod 755 "$RT/$(echo "$f" | tr 'a-z' 'A-Z')" 2>/dev/null
done
to_crlf "$SRC/tools/winpkg/HOWTO-PLAY.txt"   "$RT/HOWTO-PLAY.TXT"
to_crlf "$SRC/tools/winpkg/README-FIRST.txt" "$RT/README-FIRST.TXT"
cp -f "$SRC/RUNBOOK.md" "$RT/RUNBOOK.MD"
for m in "$BINS/MANIFEST.txt" "$BINS/../MANIFEST.txt"; do
	[ -f "$m" ] && { cp -f "$m" "$RT/MANIFEST.TXT"; break; }
done

# ------------------------------------------------------------------- DLLs ---
# The mingw build links dynamically against the mingw runtime (winpthread,
# libstdc++, libgcc) and against openssl/pcre.  Windows looks in the
# executable's directory and then in PATH, so the DLLs go to the package
# root and START-ALL.BAT prepends that root to PATH.
search_dirs() {
	[ -n "${DLL_PATH:-}" ] && echo "$DLL_PATH"
	[ -n "${MINGW_PREFIX:-}" ] && echo "$MINGW_PREFIX/bin"
	for d in /mingw64/bin /ucrt64/bin /clang64/bin /usr/bin; do echo "$d"; done
}

# DLLs that ship with Windows itself - never copied, never reported missing.
is_system_dll() {
	case "$(echo "$1" | tr 'A-Z' 'a-z')" in
		api-ms-*|ext-ms-*|kernel32.dll|kernelbase.dll|ntdll.dll|msvcrt.dll|\
		advapi32.dll|user32.dll|gdi32.dll|shell32.dll|shlwapi.dll|ole32.dll|\
		oleaut32.dll|ws2_32.dll|wsock32.dll|mswsock.dll|iphlpapi.dll|\
		bcrypt.dll|bcryptprimitives.dll|crypt32.dll|secur32.dll|sspicli.dll|\
		psapi.dll|userenv.dll|wldap32.dll|netapi32.dll|rpcrt4.dll|version.dll|\
		winmm.dll|dbghelp.dll|imm32.dll|comctl32.dll|comdlg32.dll|setupapi.dll|\
		ucrtbase.dll|normaliz.dll|dnsapi.dll|powrprof.dll|wintrust.dll) return 0 ;;
	esac
	return 1
}

say "collecting the runtime DLLs the binaries import"
DLLS_COPIED=0
DLLS_MISSING=""
if command -v objdump >/dev/null 2>&1; then
	IMPORTS="$(for e in "$RT"/*/*.exe; do
			objdump -p "$e" 2>/dev/null | awk '/DLL Name/{print $NF}'
		done | sort -u)"
	for d in $IMPORTS; do
		is_system_dll "$d" && continue
		[ -f "$RT/$d" ] && continue
		found=""
		for dir in $(search_dirs); do
			[ -f "$dir/$d" ] && { found="$dir/$d"; break; }
		done
		if [ -n "$found" ]; then
			cp -f "$found" "$RT/$d" && DLLS_COPIED=$((DLLS_COPIED+1)) \
				&& say "  $(basename "$found")"
		else
			DLLS_MISSING="$DLLS_MISSING $d"
		fi
	done
else
	say "  objdump not found - cannot derive the import list."
	say "  Copy these next to the .exe files by hand, from your mingw bin dir:"
	say "    libwinpthread-1.dll libstdc++-6.dll libgcc_s_seh-1.dll"
	say "    libcrypto-3-x64.dll libpcre-1.dll"
	DLLS_MISSING=" (import list unavailable)"
fi
[ -n "$DLLS_MISSING" ] && say "  WARNING: not found in the search path:$DLLS_MISSING"
say "  $DLLS_COPIED runtime DLL(s) packaged"

# a record of what the package contains
{
	echo "idealworld-152 server - Windows one-click package"
	echo "================================================="
	echo "packaged  : $(date -u +'%Y-%m-%dT%H:%M:%SZ')"
	echo "commit    : $(cd "$SRC" && git rev-parse --short HEAD 2>/dev/null || echo unknown)"
	echo "binaries  : $BINS"
	echo
	echo "runtime DLLs copied into the package root:"
	if [ "$DLLS_COPIED" -gt 0 ]; then ls -1 "$RT"/*.dll 2>/dev/null | sed 's|.*/|  |'
	else echo "  (none - see the packaging log)"; fi
	echo
	echo "NOT included (you must supply these, see HOWTO-PLAY.TXT):"
	echo "  - the PW 1.5.2 game data package  -> gs\\data\\"
	echo "  - a PW 1.5.2 era game client"
} > "$RT/PACKAGE.TXT"

# ------------------------------------------------------------------- zip ----
OUT_ABS="$OUT"; case "$OUT" in /*) ;; *) OUT_ABS="$PWD/$OUT" ;; esac
say "creating $OUT_ABS"
rm -f "$OUT_ABS"
if command -v zip >/dev/null 2>&1; then
	( cd "$STAGE" && zip -qry "$OUT_ABS" "$NAME" ) || die "zip failed"
elif command -v cygpath >/dev/null 2>&1 && command -v powershell.exe >/dev/null 2>&1; then
	powershell.exe -NoProfile -Command \
		"Compress-Archive -Path '$(cygpath -w "$RT")\\*' -DestinationPath '$(cygpath -w "$OUT_ABS")' -Force" \
		|| die "Compress-Archive failed"
elif command -v python3 >/dev/null 2>&1; then
	python3 -c 'import shutil,sys; shutil.make_archive(sys.argv[1][:-4], "zip", root_dir=sys.argv[2], base_dir=sys.argv[3])' \
		"$OUT_ABS" "$STAGE" "$NAME" || die "python zip failed"
else
	die "no zip, PowerShell or python3 found - install one of them (pacman -S zip)"
fi
[ -s "$OUT_ABS" ] || die "no archive produced"

say "done: $OUT_ABS ($(du -h "$OUT_ABS" | cut -f1))"
say "contents:"
if command -v unzip >/dev/null 2>&1; then
	unzip -l "$OUT_ABS" | awk 'NR>3 && NF>=4 {print "  " $4}' | grep -v '^  $' | head -n 40
else
	( cd "$STAGE" && find "$NAME" -maxdepth 2 | sed 's|^|  |' | head -n 40 )
fi
