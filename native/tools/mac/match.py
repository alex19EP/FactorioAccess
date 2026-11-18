"""Match the names the DLL resolves on Windows (MSVC) against the Mac arm64 symbol table (Itanium)."""
import re, sys, json, collections

W = sys.argv[1]


def split_top(s):
    """Qualified name before the first '(' outside <>, the parameter string, and what follows."""
    depth = 0
    for i, c in enumerate(s):
        if c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
        elif c == '(' and depth == 0:
            if s[:i].rstrip().endswith('operator'):
                continue
            d = 0
            for j in range(i, len(s)):
                if s[j] == '(':
                    d += 1
                elif s[j] == ')':
                    d -= 1
                    if d == 0:
                        break
            return s[:i], s[i + 1:j], s[j + 1:]
    return s, None, ''


def norm(t):
    t = t.replace('std::__1::', 'std::')
    for w in ('class ', 'struct ', 'enum ', 'union ', '__ptr64', '__cdecl'):
        t = t.replace(w, '')
    t = t.replace('unsigned __int64', 'unsigned long').replace('__int64', 'long')
    t = re.sub(r'\[abi:[^\]]*\]', '', t)
    t = re.sub(r'\(\w+\)(-?\d+)', r'\1', t)  # (TargeterType)0 -> 0
    t = re.sub(r'(?<=[<,\s])(-?\d+)(?:ull|ul|u|l)(?=\s*[,>])', r'\1', t)  # 1ul -> 1
    return re.sub(r'\s+', '', t)


def params(p):
    if p is None:
        return None
    out, depth, cur = [], 0, ''
    for c in p:
        if c in '<(':
            depth += 1
        if c in '>)':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur)
    out = [norm(x) for x in out]
    return [] if out == ['void'] else out


def drop_return_type(q):
    """Itanium prints template functions with their return type first."""
    depth, cut = 0, 0
    for i, ch in enumerate(q):
        if ch == '<':
            depth += 1
        elif ch == '>':
            depth -= 1
        elif ch == ' ' and depth == 0 and not q[:i].endswith('operator'):
            cut = i + 1
    return q[cut:]


def qnorm(q):
    q = norm(q)
    parts, depth, cur = [], 0, ''
    i = 0
    while i < len(q):
        c = q[i]
        if c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
        if q.startswith('::', i) and depth == 0:
            parts.append(cur)
            cur = ''
            i += 2
            continue
        cur += c
        i += 1
    parts.append(cur)
    base = lambda s: s.split('<')[0]
    if len(parts) >= 2:
        last, cls = parts[-1], base(parts[-2])
        if last.startswith(("`scalardeletingdestructor'", "`scalardeletingdtor'")):  # undname, llvm-undname
            last = '~' + cls
        elif last.startswith('`vftable'):
            last = 'vftable'
        elif base(last) == cls:
            last = cls
        elif last.startswith('~') and base(last[1:]) == cls:
            last = '~' + cls
        parts[-1] = last
    return '::'.join(parts)


mac = collections.defaultdict(list)
SKIP = ('typeinfo', 'non-virtual thunk', 'virtual thunk', 'guard variable', 'construction vtable', 'VTT for')
with open(f'{W}/mac_nm.txt', encoding='utf-8', errors='replace') as nm, \
        open(f'{W}/mac_dem.txt', encoding='utf-8', errors='replace') as dm:
    for a, d in zip(nm, dm):
        d = d.rstrip('\n')
        addr = a.split()[0]
        if d.startswith('vtable for '):
            mac[qnorm(d[len('vtable for '):]) + '::vftable'].append((d, None, False, addr))
            continue
        if d.startswith(SKIP):
            continue
        if d.startswith('_') and '(' not in d:
            d = d[1:]  # a C name keeps Mach-O's underscore
        q, p, rest = split_top(d)
        mac[qnorm(drop_return_type(q))].append((d, params(p), 'const' in rest, addr))

names = json.load(open(f'{W}/names.json'))['addresses']
dem = dict(l.rstrip('\n').split('\t') for l in open(f'{W}/msvc_dem.tsv', encoding='utf-8-sig'))
res = collections.Counter()
rows = []
for field, n in names:
    if n.startswith('?'):
        d = dem[n]
        m = re.search(r'__cdecl (.*)$', d)
        if m:
            q, p, rest = split_top(m.group(1))
            ps, const = params(p), rest.strip().startswith('const')
        else:  # data: the name follows the type
            q, ps, const = drop_return_type(d.rsplit('__ptr64 ', 1)[-1]).lstrip('*&'), None, False
            if q.startswith('const '):
                q = q[6:]
        q = qnorm(q.replace('const ', '', 1) if q.startswith('const ') else q)
    else:
        q, ps, const = n[:-5] if n.endswith('_REAL') else n, None, False
    cands = mac.get(q, [])
    if len(cands) > 1 and ps is not None:
        exact = [c for c in cands if c[1] == ps and c[2] == const]
        if not exact:
            exact = [c for c in cands if c[1] is not None and len(c[1]) == len(ps) and c[2] == const]
        cands = exact or cands
    uniq = {c[0] for c in cands}
    kind = 'missing' if not cands else 'ok' if len(uniq) == 1 else 'ambiguous'
    res[kind] += 1
    rows.append({'kind': kind, 'field': field, 'name': q, 'params': ps, 'mac': sorted(uniq)[:6]})
print(res)
for r in rows:
    if r['kind'] != 'ok':
        print(r['kind'], r['field'], r['name'][:120], (r['params'] or [])[:3], r['mac'] if r['kind'] == 'ambiguous' else '')
json.dump(rows, open(f'{W}/match.json', 'w'), indent=1)
