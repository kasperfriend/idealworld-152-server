#!/usr/bin/env bash
# Copy the cskill source tree, escaping every non-ASCII byte as \xNN.
#
# The cskill sources are GBK-encoded and cskill/Makefilelib compiles them
# with -finput-charset/-fexec-charset=ISO-8859-1, which preserves every byte
# verbatim.  zig's driver rejects the ISO-8859-1 value, so the zig build
# compiles this escaped copy as UTF-8 instead: \xNN escapes name explicit
# byte values, which no input/exec charset conversion touches, so the
# emitted bytes are identical to the gcc ISO-8859-1 build.
#
# This is safe for every context high bytes appear in here (comments,
# narrow/wide string literals, multi-byte char constants): escaping never
# adds or removes newlines, and the tree uses no C++ raw strings.
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
if command -v perl >/dev/null 2>&1; then
	# Runner paths contain no spaces, so word-splitting is safe here.
	# shellcheck disable=SC2086
	perl -i -pe 's/([\x80-\xFF])/sprintf("\\x%02X", ord($1))/ge' $files
elif command -v python3 >/dev/null 2>&1; then
	python3 - $files <<'PYEOF'
import re, sys
pat = re.compile(rb'[\x80-\xff]')
for f in sys.argv[1:]:
    s = open(f, 'rb').read()
    open(f, 'wb').write(pat.sub(lambda m: b'\\x%02X' % m.group(0)[0], s))
PYEOF
else
	echo "cskill-u8: need perl or python3" >&2
	exit 1
fi
echo "cskill-u8: escaped $(echo "$files" | wc -l) files"
