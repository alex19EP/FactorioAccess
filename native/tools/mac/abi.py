"""Calling-convention differences of the game functions the DLL resolves (Windows x64 vs arm64 macOS).

The DLL declares game functions in MSVC's shapes. They differ on arm64 where a class travels by value:
- Return. MSVC returns a class from a member function through a hidden pointer after `this` (rdx);
  a free function returns one of 1, 2, 4 or 8 bytes in rax if it is POD, otherwise through a hidden
  pointer in rcx. arm64 returns a class of up to 16 bytes that is trivially copyable in x0/x1 (or
  v0-v3 for floats/doubles), otherwise through x8, a register no parameter uses.
- Parameter. MSVC passes a class of 1, 2, 4 or 8 bytes in a register, others as a pointer to a
  copy. arm64 passes a trivially copyable class of up to 16 bytes in registers, others as a pointer.
Which a class is on arm64 depends on trivial copy/move/destructor, which the PDB does not record, so
the Mac code decides returns: a function returning through x8 reads x8 before writing it.

Usage: python -I abi.py <mac factorio>   (reads work/names.json, msvc_dem.tsv, match.json, mac_*.txt)
"""
import collections
import json
import os
import re
import sys

import capstone

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)  # -I leaves the script's folder off the path
import evidence as E  # noqa: E402
import layout  # noqa: E402
import tpi  # noqa: E402

WORK = E.WORK


def split_params(p):
    out, depth, cur = [], 0, ''
    for c in p:
        if c in '<(':
            depth += 1
        elif c in '>)':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return [] if out == ['void'] else out


def signature(dem):
    """(return type, parameter types, is member) from an undname line, or None for data."""
    m = re.match(r'^(?:(public|private|protected): )?(virtual |static )?(.*?) ?__cdecl (.*)$', dem)
    if not m:
        return None
    rest = m.group(4)
    depth = 0
    for i, c in enumerate(rest):
        if c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
        elif c == '(' and depth == 0 and not rest[:i].endswith('operator'):
            break
    else:
        return None
    j, d = i, 0
    for j in range(i, len(rest)):
        d += rest[j] == '('
        d -= rest[j] == ')'
        if d == 0:
            break
    member = m.group(1) is not None and m.group(2) != 'static '
    return m.group(3), split_params(rest[i + 1:j]), member


def class_of(t):
    """The TPI index of a by-value class type string, or None (pointer, reference, primitive, enum)."""
    t = t.replace('__ptr64', '').strip()
    t = re.sub(r'\s*\b(const|volatile)$', '', t)  # a const pointer: `X *const`
    if t.endswith(('*', '&')) or t.startswith('enum ') or ' enum ' in t:
        return None
    t = re.sub(r'^(const |volatile )+', '', t).replace(' const', '').strip()
    m = re.match(r'^(class|struct|union) (.*)$', t)
    if not m:
        return None  # primitive
    name = re.sub(r'\b(class|struct|union|enum) ', '', m.group(2))
    # The PDB's spelling, which undname.exe shares: `a,b<c> >`; llvm-undname writes `a, b<c>>`.
    name = re.sub(r',\s+', ',', name)
    while '>>' in name:
        name = name.replace('>>', '> >')
    return layout.by_name(name) or ('unknown', name)


def x8_in(addr, code):
    """True when the arm64 code reads x8 before writing it: a return through x8."""
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    md.detail = True
    for ins in md.disasm(code, addr):
        read, written = ins.regs_access()
        names_r = {ins.reg_name(r) for r in read}
        names_w = {ins.reg_name(r) for r in written}
        if names_r & {'x8', 'w8'}:
            return True
        if names_w & {'x8', 'w8'} or ins.mnemonic in ('bl', 'blr', 'ret', 'br'):
            return False
    return False


def main():
    mac_bin = sys.argv[1]
    layout.init(tpi.load(os.path.join(WORK, 'tpi.txt'), os.path.join(WORK, 'tpi.pkl')))
    sys.setrecursionlimit(20000)
    names = json.load(open(os.path.join(WORK, 'names.json')))['addresses']
    dem = dict(l.rstrip('\n').split('\t') for l in open(os.path.join(WORK, 'msvc_dem.tsv'), encoding='utf-8-sig'))
    match = {r['field']: r for r in json.load(open(os.path.join(WORK, 'match.json')))}

    data, foff, vmaddr, vmsize = E.macho(mac_bin)
    addr_of = {}
    with open(os.path.join(WORK, 'mac_nm.txt'), encoding='utf-8', errors='replace') as nm, \
            open(os.path.join(WORK, 'mac_dem.txt'), encoding='utf-8', errors='replace') as dm:
        for a, d in zip(nm, dm):
            p = a.split()
            if p[1] in 'tT':
                addr_of.setdefault(d.rstrip('\n'), int(p[0], 16))

    def mac_code(field):
        r = match.get(field)
        if not r or r['kind'] != 'ok':
            return None
        va = addr_of.get(r['mac'][0])
        if va is None or not (vmaddr <= va < vmaddr + vmsize):
            return None
        return va, data[foff + va - vmaddr:foff + va - vmaddr + 256]

    stats = collections.Counter()
    rows = []
    for field, n in names:
        if not n.startswith('?') or n not in dem:
            continue
        sig = signature(dem[n])
        if sig is None:
            continue
        stats['functions'] += 1
        ret, params, member = sig
        notes = []
        rc = class_of(ret)
        if rc is not None:
            if isinstance(rc, tuple):
                notes.append(f'returns {rc[1]}: type not in the PDB')
            else:
                ws, ms = layout.T[rc]['size'], layout.mac(rc).size
                win = 'hidden pointer (rdx)' if member else (
                    'rax' if ws in (1, 2, 4, 8) and 'has ctor / dtor' not in layout.T[rc]['opts'] else 'hidden pointer (rcx)')
                code = mac_code(field)
                if code is None:
                    mac = 'x8' if ms > 16 else 'unknown (no Mac code)'
                else:
                    mac = 'x8' if x8_in(*code) else ('registers' if ms <= 16 else 'x8?')
                stats[f'returns class: windows {win.split(" (")[0]}, mac {mac}'] += 1
                notes.append(f'returns {layout.name_of(rc)} ({ws}/{ms} bytes): windows {win}, mac {mac}')
        for k, p in enumerate(params):
            pc = class_of(p)
            if pc is None:
                continue
            if isinstance(pc, tuple):
                notes.append(f'param {k + 1} {pc[1]} by value: type not in the PDB')
                continue
            # Small classes (tags, IDs, positions) are taken as trivially copyable: one register on
            # both. A non-trivial one would go by pointer on arm64; the PDB cannot show which.
            ws, ms = layout.T[pc]['size'], layout.mac(pc).size
            win = 'register' if ws in (1, 2, 4, 8) else 'pointer to copy'
            mac = 'pointer to copy' if ms > 16 else 'register' if ms <= 8 else 'two registers (or v0-v3 for floats)'
            same = win == mac
            stats['by-value class param: ' + ('same' if same else 'differs')] += 1
            if not same:
                notes.append(f'param {k + 1} {layout.name_of(pc)} ({ws}/{ms} bytes) by value: windows {win}, mac {mac}')
        if any('long' in x and 'long long' not in x for x in [ret] + params):
            notes.append('uses long (4 bytes on Windows, 8 on Mac)')
            stats['uses long'] += 1
        if notes:
            rows.append({'field': field, 'function': dem[n], 'notes': notes})
    for r in rows:
        print(f"{r['field']}: {re.sub(r' __ptr64', '', r['function'])[:150]}")
        for x in r['notes']:
            print(f'    {x}')
    print('--')
    for k, v in sorted(stats.items()):
        print(f'  {k}: {v}')
    json.dump(rows, open(os.path.join(WORK, 'abi.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
