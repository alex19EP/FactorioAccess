"""List every field offset, type size and enum value native/src/game.cpp resolves from the PDB.

Usage: python -I lookups.py <repo>/native/src/game.cpp > work/lookups.json
"""
import json
import re
import sys

text = open(sys.argv[1], encoding="utf-8").read()
S = r'((?:"[^"]*"\s*)+)'
join = lambda s: "".join(re.findall(r'"([^"]*)"', s))
field = r'(?:layout\.)?([\w\[\]\.]+)'

out = {
    "offsets": [(f, join(t), join(p)) for f, t, p in re.findall(r'\boffset\(\s*' + field + r'\s*,\s*' + S + r',\s*' + S + r'\)', text)],
    "sizes": [(f, join(t)) for f, t in re.findall(r'\bsize\(\s*' + field + r'\s*,\s*' + S + r'\)', text)],
    "enums": [(f, join(t), join(n)) for f, t, n in re.findall(r'\benumerator\(\s*' + field + r'\s*,\s*' + S + r',\s*' + S + r'\)', text)],
}
json.dump(out, sys.stdout, indent=0)
print({k: len(v) for k, v in out.items()}, file=sys.stderr)
