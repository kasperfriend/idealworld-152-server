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
python3 - "$dst" <<'PYEOF_INNER'
import os, sys
n = 0
dst = sys.argv[1]
want = ('.cpp', '.h', '.hpp')
for dp, dn, fns in os.walk(dst):
  for fn in fns:
    if not fn.endswith(want): continue
    f = os.path.join(dp, fn)
    raw = open(f, 'rb').read()
    if not any(c >= 0x80 for c in raw): continue

    try:
        txt = raw.decode('gbk')
    except UnicodeDecodeError as e:
        print(f"cskill-u8: WARNING {f}: {e}; using replace", flush=True)
        txt = raw.decode('gbk', errors='replace')
    open(f, 'wb').write(txt.encode('utf-8'))
    n += 1
print(f"cskill-u8: transcoded {n} files")
if n == 0:
    print("cskill-u8: ERROR no non-ASCII files found", flush=True)
    sys.exit(1)
PYEOF_INNER
