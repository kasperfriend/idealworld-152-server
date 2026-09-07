#!/usr/bin/env bash
# Copy the cskill source tree, transcoding GBK sources to UTF-8.
#
# The cskill sources are GBK-encoded and cskill/Makefilelib compiles them
# with -finput-charset/-fexec-charset=ISO-8859-1, which preserves every byte
# verbatim.  zig's driver rejects the ISO-8859-1 value (and does not forward
# -Xclang -fexec-charset either), and clang hard-errors on non-UTF-8 bytes in
# string literals, so the zig build compiles this UTF-8 copy instead.
#
# NOTE: narrow-string bytes change (GBK 2-byte chars become UTF-8 3-byte
# chars), so zig-built servers send mojibake for Chinese skill names.
# The native (MSYS2 gcc) build reads the GBK in place and stays byte-exact;
# prefer it for production.  This only affects display strings.
set -u
src="$1"
dst="$2"
rm -rf "$dst"
cp -r "$src" "$dst"
files=$(grep -rlP '[\x80-\xFF]' "$dst/skill" "$dst/skills" "$dst/header" "$dst/expr" \
	--include='*.cpp' --include='*.h' --include='*.hpp' || true)
if [ -z "$files" ]; then
	echo "cskill-u8: no non-ASCII bytes found (unexpected)"
	exit 0
fi
python3 - $files <<'PYEOF_INNER'
import sys
n = 0
for f in sys.argv[1:]:
    raw = open(f, 'rb').read()
    try:
        txt = raw.decode('gbk')
    except UnicodeDecodeError as e:
        print(f"cskill-u8: WARNING {f}: {e}; using replace", flush=True)
        txt = raw.decode('gbk', errors='replace')
    open(f, 'wb').write(txt.encode('utf-8'))
    n += 1
print(f"cskill-u8: transcoded {n} files")
PYEOF_INNER
