"""Parse `llvm-pdbutil dump -types` text into a compact type table, cached as a pickle.

Records kept: classes, structs, unions, enums, field lists, pointers, modifiers, arrays, bitfields.
A type reference is an int; a primitive (< 0x1000) keeps its printed name in PRIM.
"""
import os
import pickle
import re
import sys

REC = re.compile(r'^\s+0x([0-9A-F]+) \| (LF_\w+) \[size = \d+\](?: `(.*)`)?\s*$')
REF = re.compile(r'0x([0-9A-F]+)(?: \(([^)]*)\))?')
PRIM = {}


def ref(s):
    if s.strip() == '<no type>':
        return None
    m = REF.match(s.strip())
    v = int(m.group(1), 16)
    if v < 0x1000 and m.group(2):
        PRIM[v] = m.group(2)
    return v


def parse(path):
    types = {}
    cur = None  # (idx, kind, name, lines)

    def flush():
        if cur is None:
            return
        idx, kind, name, lines = cur
        body = '\n'.join(lines)
        if kind in ('LF_STRUCTURE', 'LF_CLASS', 'LF_UNION', 'LF_INTERFACE'):
            fl = re.search(r'field list: (<no type>|0x[0-9A-F]+)', body)
            opts = re.search(r'options: (.*), sizeof (\d+)', body)
            uniq = re.search(r'unique name: `([^`]*)`', body)
            vt = re.search(r'vtable: (<no type>|0x[0-9A-F]+)', body)
            types[idx] = {
                'k': 'union' if kind == 'LF_UNION' else 'class',
                'name': name,
                'fl': None if fl.group(1) == '<no type>' else int(fl.group(1), 16),
                'size': int(opts.group(2)),
                'opts': opts.group(1),
                'fwd': 'forward ref' in opts.group(1),
                'uniq': uniq.group(1) if uniq else name,
            }
        elif kind == 'LF_ENUM':
            fl = re.search(r'underlying type: (0x[0-9A-F]+(?: \([^)]*\))?|<no type>)', body)
            opts = re.search(r'options: (.*)', body)
            uniq = re.search(r'unique name: `([^`]*)`', body)
            types[idx] = {'k': 'enum', 'name': name, 'under': ref(fl.group(1)),
                          'fwd': 'forward ref' in opts.group(1), 'uniq': uniq.group(1) if uniq else name}
        elif kind == 'LF_POINTER':
            r = re.search(r'referent = (0x[0-9A-F]+(?: \([^)]*\))?|<no type>)', body)
            mode = re.search(r'mode = ([\w ]+?),', body)
            mode = mode.group(1) if mode else '?'
            types[idx] = {'k': 'ptr', 'to': ref(r.group(1)) if r else None, 'mode': mode, 'member': 'member' in mode}
        elif kind == 'LF_MODIFIER':
            m = re.search(r'referent = (0x[0-9A-F]+(?: \([^)]*\))?|<no type>)', body)
            types[idx] = {'k': 'mod', 'to': ref(m.group(1))}
        elif kind == 'LF_ARRAY':
            m = re.search(r'size: (\d+), index type: .*?, element type: (0x[0-9A-F]+(?: \([^)]*\))?|<no type>)', body)
            types[idx] = {'k': 'array', 'bytes': int(m.group(1)), 'elem': ref(m.group(2))}
        elif kind == 'LF_BITFIELD':
            m = re.search(r'type = (0x[0-9A-F]+(?: \([^)]*\))?|<no type>), bit offset = (\d+), # bits = (\d+)', body)
            types[idx] = {'k': 'bits', 'type': ref(m.group(1)), 'pos': int(m.group(2)), 'n': int(m.group(3))}
        elif kind == 'LF_FIELDLIST':
            items = []
            for it in re.split(r'\n\s+- ', '\n' + body)[1:]:
                head = it.split()[0]
                if head == 'LF_MEMBER':
                    m = re.match(r'LF_MEMBER \[name = `(.*)`, Type = (0x[0-9A-F]+(?: \([^)]*\))?|<no type>), offset = (\d+), attrs = (\w+)', it)
                    items.append(('m', m.group(1), ref(m.group(2)), int(m.group(3)), m.group(4)))
                elif head == 'LF_BCLASS':
                    m = re.search(r'type = (0x[0-9A-F]+), offset = (\d+), attrs = (\w+)', it)
                    items.append(('b', ref(m.group(1)), int(m.group(2)), m.group(3)))
                elif head in ('LF_VBCLASS', 'LF_IVBCLASS'):
                    m = re.search(r'base = (0x[0-9A-F]+)', it)
                    items.append(('vb' if head == 'LF_VBCLASS' else 'ivb', ref(m.group(1))))
                elif head == 'LF_VFUNCTAB':
                    items.append(('vf',))
                elif head == 'LF_ENUMERATE':
                    m = re.match(r'LF_ENUMERATE \[(.*) = (-?\d+)\]', it)
                    if m:
                        items.append(('e', m.group(1), int(m.group(2))))
                elif head == 'LF_INDEX':
                    m = re.search(r'continuation = (0x[0-9A-F]+)', it) or re.search(r'(0x[0-9A-F]+)', it)
                    items.append(('cont', int(m.group(1), 16)))
                elif head == 'LF_ONEMETHOD' or head == 'LF_METHOD':
                    if 'virtual' in it or 'intro' in it:
                        items.append(('virt',))
            types[idx] = {'k': 'fields', 'items': items}

    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = REC.match(line)
            if m:
                flush()
                cur = (int(m.group(1), 16), m.group(2), m.group(3), [])
            elif cur is not None:
                cur[3].append(line.rstrip('\n'))
        flush()
    return types


def load(dump, cache):
    if os.path.exists(cache) and os.path.getmtime(cache) > os.path.getmtime(dump):
        with open(cache, 'rb') as f:
            types, prim = pickle.load(f)
        PRIM.update(prim)
        return types
    types = parse(dump)
    with open(cache, 'wb') as f:
        pickle.dump((types, dict(PRIM)), f, protocol=pickle.HIGHEST_PROTOCOL)
    return types


if __name__ == '__main__':
    t = load(sys.argv[1], sys.argv[2])
    import collections
    print(len(t), collections.Counter(v['k'] for v in t.values()))
