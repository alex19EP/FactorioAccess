"""Write the Mac symbol table: every name, offset, size, enumerator and virtual slot the DLL asks for,
with its Mac value, in the format of the DLL's symbol cache (native/src/symbols.cpp):

    mac rules <n> builds <LC_UUID of the arm64 slice> [<LC_UUID> ...]
    name <decorated name>\t<Mac symbol name as in the symbol table, e.g. _ZN4agui3Gui5logicEb>
    off <type> <path>\t<offset>
    size <type>\t<size>
    enum <type> <enumerator>\t<value>
    vslot <type> <method>\t<index from the vtable pointer>

Keys are the Windows queries, so game.cpp's resolve runs unchanged on the Mac. Functions and globals
are named, not addressed: the shipped binary keeps its symbol table, so the DLL finds them itself and
only the layout entries depend on the build. The table holds for every build listed in its header;
a build whose layouts are unchanged is added there instead of getting a table of its own. A query
with no Mac answer is left out (the DLL then stands down) and listed in the report.

Usage: python -I table.py <mac factorio> <output file>   (reads the work folder: FA_MAC_WORK)
"""
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)  # -I leaves the script's folder off the path
import evidence as E  # noqa: E402
import layout  # noqa: E402
import tpi  # noqa: E402

WORK = E.WORK
RULES = 2  # symbols.cpp's kResolverRules


def macho_uuid(path):
    """LC_UUID of the arm64 slice, as the DLL prints it: 32 uppercase hex digits."""
    data = open(path, 'rb').read()
    base = 0
    if struct.unpack_from('>I', data, 0)[0] == 0xCAFEBABE:
        for k in range(struct.unpack_from('>I', data, 4)[0]):
            cpu, _, off, _, _ = struct.unpack_from('>IIIII', data, 8 + k * 20)
            if cpu == 0x0100000C:
                base = off
    magic, _, _, _, ncmds = struct.unpack_from('<IiiII', data, base)
    assert magic == 0xFEEDFACF, 'not a 64-bit Mach-O'
    p = base + 32
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<II', data, p)
        if cmd == 0x1B:  # LC_UUID
            return data[p + 8:p + 24].hex().upper()
        p += size
    sys.exit('no LC_UUID')


def main():
    mac_bin, out_path = sys.argv[1], sys.argv[2]
    w = lambda name: os.path.join(WORK, name)  # noqa: E731
    layout.init(tpi.load(w('tpi.txt'), w('tpi.pkl')))
    sys.setrecursionlimit(20000)
    names = json.load(open(w('names.json')))
    lookups = json.load(open(w('lookups.json')))
    predicted = json.load(open(w('predicted.json')))
    match = {r['field']: r for r in json.load(open(w('match.json')))}
    slots = {(r['class'], r['method']): r['slot'] for r in json.load(open(w('slots.json')))}

    # Demangled name -> the symbol's raw name, and how many defined symbols carry each raw name.
    symbol_of, count = {}, {}
    with open(w('mac_nm.txt'), encoding='utf-8', errors='replace') as nm, \
            open(w('mac_dem.txt'), encoding='utf-8', errors='replace') as dm:
        for line, d in zip(nm, dm):
            parts = line.rstrip('\n').split(' ', 2)
            if len(parts) < 3:
                continue
            raw, d = parts[2], d.rstrip('\n')
            count[raw] = count.get(raw, 0) + 1
            symbol_of.setdefault(d, raw)
            if d.startswith('_') and '(' not in d:  # a C name: match.py drops Mach-O's underscore
                symbol_of.setdefault(d[1:], raw)

    table, missing = {}, []
    for field, name in names['addresses']:
        r = match.get(field)
        raw = symbol_of.get(r['mac'][0]) if r and r['kind'] == 'ok' else None
        if raw and count[raw] == 1:
            table[f'name {name}'] = raw
        elif raw:
            missing.append(f'name {name} ({count[raw]} symbols named {raw})')
        else:
            missing.append(f'name {name} ({r["kind"] if r else "no match"})')
    for cls, method, field in names['slots']:
        if (cls, method) in slots:
            table[f'vslot {cls} {method}'] = slots[(cls, method)]
        else:
            missing.append(f'vslot {cls} {method}')
    containers = []
    for o in predicted['offsets']:
        key = f"off {o['type']} {o['path']}"
        if 'error' not in o:
            table[key] = o['mac']
            continue
        # A path into MSVC's standard library (`queue._Mypair._Myval2._Map`): libc++ has none of
        # it, so the Mac reader (stdabi) starts at the container itself.
        parts = layout.split_path(o['path'])
        cut = next((k for k, part in enumerate(parts) if part.startswith(('_My', 'std::'))), None)
        found = layout.find(layout.by_name(o['type']), '.'.join(parts[:cut])) if cut else None
        if found:
            table[key] = found[1]
            containers.append(f"{key}: the container at {found[1]:#x}")
        else:
            missing.append(f"{key} ({o['error']})")
    for s in predicted['sizes']:
        table[f"size {s['type']}"] = s['mac']
    for field, ty, enumerator in lookups['enums']:
        i = layout.by_name(ty)
        value = next((it[2] for it in layout.fields(i) if it[0] == 'e' and it[1] == enumerator), None) if i else None
        if value is None:
            missing.append(f'enum {ty} {enumerator}')
        else:
            table[f'enum {ty} {enumerator}'] = value & 0xFFFFFFFFFFFFFFFF

    with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(f'mac rules {RULES} builds {macho_uuid(mac_bin)}\n')
        for key in sorted(table):
            f.write(f'{key}\t{table[key]}\n')
    print(f'{len(table)} entries, {len(missing)} without a Mac value, {len(containers)} read as libc++ containers')
    for c in containers:
        print(f'  container {c}')
    for m in missing:
        print(f'  missing {m}')


if __name__ == '__main__':
    main()
