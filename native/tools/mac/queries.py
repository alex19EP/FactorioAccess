"""Turn the DLL's query list (fa_symbols_check --queries) into the inputs of the other steps:
names.json (names and virtual slots) and lookups.json (offsets, sizes, enumerators). Each entry's
field is its symbol-cache key, which table.py writes back with the Mac value.

Usage: python -I queries.py <queries.tsv> <work folder>
"""
import json
import os
import sys

src, work = sys.argv[1], sys.argv[2]
names = {'addresses': [], 'slots': []}
lookups = {'offsets': [], 'sizes': [], 'enums': []}
for line in open(src, encoding='utf-8'):
    kind, first, second, _ = line.rstrip('\n').split('\t')
    key = f'{kind} {first} {second}' if second else f'{kind} {first}'
    if kind == 'rva':
        names['addresses'].append((key, first))
    elif kind == 'vslot':
        names['slots'].append((first, second, key))
    elif kind == 'off':
        lookups['offsets'].append((key, first, second))
    elif kind == 'size':
        lookups['sizes'].append((key, first))
    elif kind == 'enum':
        lookups['enums'].append((key, first, second))
    else:
        sys.exit(f'unknown query kind {kind}')
json.dump(names, open(os.path.join(work, 'names.json'), 'w'), indent=0)
json.dump(lookups, open(os.path.join(work, 'lookups.json'), 'w'), indent=0)
print({k: len(v) for k, v in {**names, **lookups}.items()})
