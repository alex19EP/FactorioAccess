"""Find each virtual slot the DLL asks for in the Mac arm64 vtables (chained-fixup pointers)."""
import json, re, struct, sys, collections, bisect

W, BIN = sys.argv[1], sys.argv[2]
data = open(BIN, 'rb').read()

# Universal header: pick the arm64 slice.
assert struct.unpack_from('>I', data, 0)[0] == 0xCAFEBABE
n = struct.unpack_from('>I', data, 4)[0]
slice_off = None
for i in range(n):
    cpu, sub, off, size, align = struct.unpack_from('>IIIII', data, 8 + i * 20)
    if cpu == 0x0100000C:
        slice_off = off
mh = slice_off
magic, cpu, sub, ftype, ncmds, sizeofcmds, flags, _ = struct.unpack_from('<IiiIIIII', data, mh)
assert magic == 0xFEEDFACF
segs = []
p = mh + 32
for _ in range(ncmds):
    cmd, size = struct.unpack_from('<II', data, p)
    if cmd == 0x19:  # LC_SEGMENT_64
        name = data[p + 8:p + 24].rstrip(b'\0').decode()
        vmaddr, vmsize, fileoff, filesize = struct.unpack_from('<QQQQ', data, p + 24)
        segs.append((name, vmaddr, vmsize, fileoff, filesize))
    p += size
base = next(s[1] for s in segs if s[0] == '__TEXT')


def read_ptr(vm):
    for name, va, vs, fo, fs in segs:
        if va <= vm < va + fs:
            # Plain absolute pointers (classic rebase info, no chained fixups); an imported
            # function (__cxa_pure_virtual) is bound at load time and reads 0 in the file.
            raw = struct.unpack_from('<Q', data, slice_off + fo + (vm - va))[0]
            if raw == 0:
                return ('bind', 0)
            return ('rebase', raw)
    return (None, None)


names, vtables, addrs = {}, {}, []
with open(f'{W}/mac_nm.txt', encoding='utf-8', errors='replace') as nm, \
        open(f'{W}/mac_dem.txt', encoding='utf-8', errors='replace') as dm:
    for a, d in zip(nm, dm):
        va = int(a.split()[0], 16)
        d = d.rstrip('\n')
        addrs.append(va)
        if d.startswith('vtable for '):
            vtables[d[len('vtable for '):]] = va
        elif a.split()[1] in 'tT':
            names.setdefault(va, d)
addrs = sorted(set(addrs))


def entries(cls):
    va = vtables[cls]
    end = addrs[bisect.bisect_right(addrs, va)]
    out = []
    for slot, at in enumerate(range(va + 16, end, 8)):  # skip offset-to-top and typeinfo
        kind, target = read_ptr(at)
        if kind == 'rebase' and target in names:
            out.append(names[target])
        elif kind == 'bind':
            out.append('<import>')
        else:
            break  # the primary vtable ends (secondary vtables start with offset-to-top)
    return out


def method_of(d):
    q = d.split('(')[0]
    return q.rsplit('::', 1)[-1] if '::' in q else q


req = json.load(open(f'{W}/names.json'))['slots']
result = collections.Counter()
found = {}  # (class, method) -> Mac slot index, written to slots.json
for cls, method, field in req:
    if cls not in vtables:
        result['no vtable'] += 1
        print('NO VTABLE', cls, method)
        continue
    es = entries(cls)
    hits = [i for i, e in enumerate(es) if method_of(e) == method]
    if len(hits) == 1 or (method.startswith('~') and len(hits) == 2 and hits[1] == hits[0] + 1):
        result['ok'] += 1
        found[(cls, method)] = hits[0]
        print('ok', cls, method, hits, es[hits[0]][:90])
    elif not hits:
        result['not in vtable'] += 1
        print('NOT FOUND', cls, method, f'{len(es)} entries, {es.count("<import>")} pure/imported')
    else:
        result['overloaded'] += 1
        print('OVERLOADED', cls, method, [(i, es[i][:70]) for i in hits])
print(result)


# --- pure virtuals and interface bases: read every sub-vtable of every class ---
def segments(cls):
    va = vtables[cls]
    end = addrs[bisect.bisect_right(addrs, va)]
    segs_, cur, at = [], None, va
    while at + 16 <= end:
        kind, target = read_ptr(at)
        k2, ti = read_ptr(at + 8)
        # A sub-vtable starts with offset-to-top (0 or negative) and a typeinfo pointer.
        raw = struct.unpack_from('<q', data, slice_off + next(fo + (at - sva) for _, sva, vs, fo, fs in segs if sva <= at < sva + fs))[0]
        if raw <= 0 and k2 == 'rebase' and ti not in names:
            cur = []
            segs_.append(cur)
            at += 16
            continue
        if cur is None:
            break
        if kind == 'rebase' and target in names:
            cur.append(names[target])
        elif kind == 'bind':
            cur.append('<import>')
        else:
            break
        at += 8
    return segs_


PURE = ('___cxa_pure_virtual', '<import>')


def same_method(a, b):
    ma, mb = method_of(a), method_of(b)
    return ma == mb or (ma.startswith('~') and mb.startswith('~'))
all_segs = None
print('\n-- second pass --')
for cls, method, field in req:
    if cls in vtables:
        es = entries(cls)
        if any(method_of(e) == method for e in es):
            continue
    if all_segs is None:
        all_segs = [s for c in vtables for s in segments(c)]
    votes = collections.Counter()
    if cls in vtables:
        es = entries(cls)
        for s in all_segs:
            if len(s) < len(es):
                continue
            if all(es[j] in PURE or same_method(s[j], es[j]) for j in range(len(es))):
                for i in range(len(es)):
                    if es[i] in PURE and method_of(s[i]) == method:
                        votes[i] += 1
    else:
        for s in all_segs:
            if any(e.startswith(cls + '::') for e in s):
                for i, e in enumerate(s):
                    if method_of(e) == method:
                        votes[i] += 1
    print('pure' if cls in vtables else 'interface', cls, method, dict(votes.most_common(3)))
    if votes:
        found[(cls, method)] = votes.most_common(1)[0][0]
json.dump([{'class': c, 'method': m, 'slot': i} for (c, m), i in sorted(found.items())],
          open(f'{W}/slots.json', 'w'), indent=1)
