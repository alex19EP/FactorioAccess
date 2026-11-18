"""Check the predicted Mac layouts against the Mac machine code.

For each non-template instance method present once by name on both platforms, collect the offsets
its code reads, writes or takes the address of relative to `this` (rcx on Windows x64, x0 on arm64,
following register copies). Member order is the same on both platforms, so when both functions
touch the same number of distinct offsets, the sorted lists pair up: Windows offset -> Mac offset.
Each pair is then checked against the layout rules: the Windows offset's member(s) must be
predicted at that Mac offset.

Usage: python -I evidence.py <factorio.exe> <mac factorio>
Reads work/win_publics.txt (llvm-pdbutil dump -publics), work/mac_nm.txt, work/mac_dem.txt,
work/tpi.txt; writes work/evidence.json.
"""
import bisect
import collections
import json
import os
import re
import struct
import sys

import capstone
import pefile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)  # -I leaves the script's folder off the path
import layout  # noqa: E402
import tpi  # noqa: E402

WORK = os.environ.get('FA_MAC_WORK') or os.path.join(HERE, 'work')
MAX_FN = 768  # bytes: small functions only; long ones touch too much to pair reliably


# --- Windows ------------------------------------------------------------------------------------
def win_functions(exe_path):
    pe = pefile.PE(exe_path, fast_load=True)
    text = pe.sections[0]
    code = text.get_data()
    base = pe.OPTIONAL_HEADER.ImageBase + text.VirtualAddress
    starts, named = set(), []
    name = None
    with open(os.path.join(WORK, 'win_publics.txt'), encoding='utf-8', errors='replace') as f:
        for line in f:
            m = re.search(r'S_PUB32 \[size = \d+\] `(.*)`', line)
            if m:
                name = m.group(1)
                continue
            m = re.search(r'addr = 0001:(\d+)', line)
            if m and name:
                off = int(m.group(1))
                starts.add(off)
                if 'function' in line:
                    named.append((name, off))
            name = None
    starts = sorted(starts)
    # ?method@Class@ns@@<access>E<cv>: a non-static member function of a non-template class.
    pat = re.compile(r'^\?([A-Za-z_]\w*)@((?:[A-Za-z_]\w*@)+)@([AEIMQU])E([AB])')
    out = collections.defaultdict(list)
    for name, off in named:
        m = pat.match(name)
        if not m:
            continue
        cls = '::'.join(reversed(m.group(2).rstrip('@').split('@')))
        nxt = starts[bisect.bisect_right(starts, off)] if off < starts[-1] else len(code)
        virtual = m.group(3) in 'EMU'
        out[(cls, m.group(1), m.group(4) == 'B')].append((base + off, code[off:off + min(nxt - off, MAX_FN)], virtual))
    return out


X64_VOLATILE = {'rax', 'rcx', 'rdx', 'r8', 'r9', 'r10', 'r11'}
X64_FULL = {}
for r in 'abcd':
    for n in (f'r{r}x', f'e{r}x', f'{r}x', f'{r}l', f'{r}h'):
        X64_FULL[n] = f'r{r}x'
for r in ('si', 'di', 'bp', 'sp'):
    for n in (f'r{r}', f'e{r}', r, f'{r}l'):
        X64_FULL[n] = f'r{r}'
for i in range(8, 16):
    for n in (f'r{i}', f'r{i}d', f'r{i}w', f'r{i}b'):
        X64_FULL[n] = f'r{i}'


def x64_accesses(addr, code):
    """(offset, size) of each memory access relative to `this`. Registers holding `this` or a
    pointer into it (mov, lea) are followed; lea itself is not an access."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    X = capstone.x86
    alias = {'rcx': 0}  # register -> offset into this
    acc = []
    for ins in md.disasm(code, addr):
        if ins.mnemonic == 'int3':
            break
        ops = ins.operands
        derived = None
        for op in ops:
            if op.type == X.X86_OP_MEM and op.mem.index == 0 and op.mem.segment == 0 and op.mem.base:
                b = X64_FULL.get(ins.reg_name(op.mem.base))
                if b in alias:
                    if ins.mnemonic == 'lea':
                        derived = alias[b] + op.mem.disp
                    else:
                        acc.append((alias[b] + op.mem.disp, op.size))
        if ins.mnemonic.startswith('call'):
            for r in X64_VOLATILE:
                alias.pop(r, None)
            continue
        if ins.mnemonic.startswith('jmp') and not any(o.type == X.X86_OP_IMM for o in ops):
            break  # tail call through a register or memory
        dst = X64_FULL.get(ins.reg_name(ops[0].reg)) if ops and ops[0].type == X.X86_OP_REG else None
        if ins.mnemonic == 'mov' and len(ops) == 2 and ops[1].type == X.X86_OP_REG and ops[0].type == X.X86_OP_REG                 and ops[0].size == 8 and X64_FULL.get(ins.reg_name(ops[1].reg)) in alias:
            alias[dst] = alias[X64_FULL[ins.reg_name(ops[1].reg)]]
            continue
        if ins.mnemonic == 'lea' and derived is not None and ops[0].size == 8:
            alias[dst] = derived
            continue
        if ins.mnemonic == 'add' and dst in alias and len(ops) == 2 and ops[1].type == X.X86_OP_IMM and ops[0].size == 8:
            alias[dst] += ops[1].imm
            continue
        _, written = ins.regs_access()
        for r in written:
            alias.pop(X64_FULL.get(ins.reg_name(r)), None)
    return acc


# --- Mac ----------------------------------------------------------------------------------------
def macho(path):
    data = open(path, 'rb').read()
    assert struct.unpack_from('>I', data, 0)[0] == 0xCAFEBABE
    slice_off = None
    for i in range(struct.unpack_from('>I', data, 4)[0]):
        cpu, _, off, _, _ = struct.unpack_from('>IIIII', data, 8 + i * 20)
        if cpu == 0x0100000C:
            slice_off = off
    _, _, _, _, ncmds, _, _, _ = struct.unpack_from('<IiiIIIII', data, slice_off)
    p = slice_off + 32
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<II', data, p)
        if cmd == 0x19 and data[p + 8:p + 24].rstrip(b'\0') == b'__TEXT':
            vmaddr, vmsize, fileoff, _ = struct.unpack_from('<QQQQ', data, p + 24)
            return data, slice_off + fileoff, vmaddr, vmsize
        p += size
    raise SystemExit('no __TEXT')


def split_top(s):
    depth = 0
    for i, c in enumerate(s):
        if c in '<':
            depth += 1
        elif c == '>':
            depth -= 1
        elif c == '(' and depth == 0:
            return s[:i], s[i:]
    return s, ''


def mac_functions(path):
    data, foff, vmaddr, vmsize = macho(path)
    rows = []
    with open(os.path.join(WORK, 'mac_nm.txt'), encoding='utf-8', errors='replace') as nm, \
            open(os.path.join(WORK, 'mac_dem.txt'), encoding='utf-8', errors='replace') as dm:
        for a, d in zip(nm, dm):
            parts = a.split()
            rows.append((int(parts[0], 16), parts[1], d.rstrip('\n')))
    starts = sorted({r[0] for r in rows})
    out = collections.defaultdict(list)
    for va, kind, d in rows:
        if kind not in 'tT' or not (vmaddr <= va < vmaddr + vmsize):
            continue
        q, rest = split_top(d)
        if not rest or '<' in q or ' ' in q:
            continue  # templates, and anything with a return type printed first
        cls, _, meth = q.rpartition('::')
        if not cls:
            continue
        nxt = starts[bisect.bisect_right(starts, va)] if va < starts[-1] else vmaddr + vmsize
        body = data[foff + va - vmaddr:foff + va - vmaddr + min(nxt - va, MAX_FN)]
        const = rest.rsplit(')', 1)[-1].strip().startswith('const')
        out[(cls.replace('std::__1::', 'std::'), meth, const)].append((va, body))
    return out


def a64_size(ins, reg):
    m = ins.mnemonic
    if m.endswith('b') and m[-2:] != 'sb' or m.endswith('sb'):
        return 1
    if m.endswith('h'):
        return 2
    if m in ('ldrsw', 'ldursw'):
        return 4
    return {'x': 8, 'w': 4, 'q': 16, 'd': 8, 's': 4, 'h': 2, 'b': 1}.get(reg[0], 0)


def a64_full(name):
    if name in ('sp', 'wsp', 'xzr', 'wzr') or not name or name[0] not in 'xw':
        return name
    return 'x' + name[1:]


def a64_accesses(addr, code):
    """(offset, size) of each memory access relative to `this` (x0), following copies and
    add-immediates (large offsets are built as add #n, lsl #12 then a load)."""
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    md.detail = True
    A = capstone.arm64
    alias = {'x0': 0}
    acc = []
    for ins in md.disasm(code, addr):
        ops = ins.operands
        m = ins.mnemonic
        mems = [o for o in ops if o.type == A.ARM64_OP_MEM]
        if mems:
            o = mems[0]
            b = a64_full(ins.reg_name(o.mem.base)) if o.mem.base else None
            if b in alias and o.mem.index == 0:
                regs = [ins.reg_name(r.reg) for r in ops if r.type == A.ARM64_OP_REG]
                sz = a64_size(ins, regs[0]) if regs else 0
                post = ins.writeback and any(r.type == A.ARM64_OP_IMM for r in ops)
                disp = alias[b] + (0 if post else o.mem.disp)
                acc.append((disp, sz))
                if m in ('ldp', 'stp', 'ldnp', 'stnp'):
                    acc.append((disp + sz, sz))
                if ins.writeback:
                    alias.pop(b, None)
        if m in ('bl', 'blr'):
            for i in range(19):
                alias.pop(f'x{i}', None)
            continue
        if m in ('b', 'br') and (m == 'br' or not ops or ops[0].type != A.ARM64_OP_IMM):
            break
        if m == 'b' and ops and ops[0].type == A.ARM64_OP_IMM and not (addr <= ops[0].imm < addr + len(code)):
            break  # tail call
        dst = a64_full(ins.reg_name(ops[0].reg)) if ops and ops[0].type == A.ARM64_OP_REG else None
        if m in ('add', 'sub') and len(ops) == 3 and ops[2].type == A.ARM64_OP_IMM and ops[1].type == A.ARM64_OP_REG                 and a64_full(ins.reg_name(ops[1].reg)) in alias and dst and dst.startswith('x'):
            sh = ops[2].shift.value if ops[2].shift.type else 0
            alias[dst] = alias[a64_full(ins.reg_name(ops[1].reg))] + (ops[2].imm << sh) * (1 if m == 'add' else -1)
            continue
        if m == 'mov' and len(ops) == 2 and ops[1].type == A.ARM64_OP_REG                 and ins.reg_name(ops[0].reg).startswith('x') and a64_full(ins.reg_name(ops[1].reg)) in alias:
            alias[dst] = alias[a64_full(ins.reg_name(ops[1].reg))]
            continue
        _, written = ins.regs_access()
        for r in written:
            alias.pop(a64_full(ins.reg_name(r)), None)
    return acc


# --- predictions --------------------------------------------------------------------------------
def flat(i, cap=4096):
    """Windows offset -> {Mac offset: [member paths]} for every member start, recursively."""
    out = collections.defaultdict(lambda: collections.defaultdict(list))

    def walk(i, wbase, mbase, path, depth):
        i = layout.real(i)
        if i is None or i < 0x1000 or depth > 8 or len(out) > cap:
            return
        t = layout.T[i]
        if t['k'] == 'array':
            ew, em = layout.win(t['elem']), layout.mac(t['elem'])
            if ew.size:
                for n in range(min(t['bytes'] // ew.size, 64)):
                    out[wbase + n * ew.size][mbase + n * em.size].append(f'{path}[{n}]')
                    walk(t['elem'], wbase + n * ew.size, mbase + n * em.size, f'{path}[{n}]', depth + 1)
            return
        if t['k'] not in ('class', 'union'):
            return
        rule = layout.std_rule(t['name'])
        if rule and rule[0] in ('table', 'uptr'):
            return
        lw, lm = layout.winmembers(i), layout.mac(i)
        for name, (wo, ty) in lw.items():
            if name not in lm.members:
                continue
            mo = lm.members[name][0]
            p = f'{path}.{name}' if path else name
            out[wbase + wo][mbase + mo].append(p)
            walk(ty, wbase + wo, mbase + mo, p, depth + 1)

    walk(i, 0, 0, '', 0)
    return out


def main():
    exe, mac_bin = sys.argv[1], sys.argv[2]
    types = tpi.load(os.path.join(WORK, 'tpi.txt'), os.path.join(WORK, 'tpi.pkl'))
    layout.init(types)
    sys.setrecursionlimit(20000)
    look = json.load(open(os.path.join(WORK, 'lookups.json')))
    wanted = {ty for _, ty, _ in look['offsets']}

    wf = win_functions(exe)
    mf = mac_functions(mac_bin)
    print(f'windows methods {len(wf)}, mac methods {len(mf)}')
    stats = collections.Counter()
    pairs = collections.defaultdict(lambda: collections.defaultdict(collections.Counter))  # cls -> woff -> moff
    for key, wl in wf.items():
        ml = mf.get(key)
        if not ml or len(wl) != 1 or len({va for va, _ in ml}) != 1:
            continue
        cls = key[0]
        ci = layout.by_name(cls)
        if ci is None:
            continue
        # MSVC passes an override of a secondary base's virtual `this` pointing at that base.
        if wl[0][2] and any(it[0] == 'b' and it[2] for it in layout.fields(ci)):
            stats['virtual, secondary bases'] += 1
            continue
        stats['methods'] += 1
        wa = sorted({o for o, _ in x64_accesses(*wl[0][:2]) if 0 <= o < 0x100000})
        ma = sorted({o for o, _ in a64_accesses(*ml[0]) if 0 <= o < 0x100000})
        if not wa or len(wa) != len(ma):
            stats['unpaired' if wa else 'no this access'] += 1
            continue
        stats['paired'] += 1
        for w, m in zip(wa, ma):
            pairs[cls][w][m] += 1

    verdict = collections.Counter()
    wanted_v = collections.Counter()
    report = []
    for cls, offs in sorted(pairs.items()):
        f = flat(layout.by_name(cls))
        for w, ms in sorted(offs.items()):
            m, n = ms.most_common(1)[0]
            if len(ms) > 1 and ms.most_common(2)[1][1] * 2 >= n:
                v = 'conflicting evidence'
            elif w not in f:
                v = 'not a member start'
            elif m in f[w]:
                v = 'agree'
            else:
                v = 'DISAGREE'
            verdict[v] += 1
            if cls in wanted:
                wanted_v[v] += 1
            report.append({'class': cls, 'win': w, 'mac': m, 'seen': dict(ms), 'verdict': v,
                           'predicted': {hex(k): p[:3] for k, p in f.get(w, {}).items()}})
    print('methods:', dict(stats))
    print('offsets, all classes:', dict(verdict))
    print('offsets, classes the DLL looks up:', dict(wanted_v))
    # The DLL's own lookups: is the predicted Mac offset among those the code shows?
    ev = {(r['class'], r['win']): r for r in report}
    pred = json.load(open(os.path.join(WORK, 'predicted.json')))
    lk = collections.Counter()
    for o in pred['offsets']:
        if 'error' in o:
            continue
        r = ev.get((o['type'], o['win']))
        if r is None:
            lk['no evidence'] += 1
            continue
        seen = r['seen']
        if o['mac'] in seen:
            lk['confirmed twice or more' if seen[o['mac']] >= 2 else 'confirmed once'] += 1
        else:
            lk['contradicted'] += 1
            print(f"  contradicted {o['type']}.{o['path']}: win {o['win']:#x}, predicted {o['mac']:#x}, "
                  f"code shows {({hex(k): v for k, v in seen.items()})}")
    print('DLL lookups:', dict(lk))
    json.dump(report, open(os.path.join(WORK, 'evidence.json'), 'w'), indent=1, default=str)


if __name__ == "__main__":
    main()
