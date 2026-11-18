"""Predict Mac arm64 (clang, libc++, Itanium C++ ABI, Apple tail-padding rule) class layouts from the
Windows PDB's types, and check a simple MSVC model against the PDB to find classes whose layout the
rules here cannot see (alignas, packing).

Every predicted layout carries flags: reasons the prediction may be wrong.
"""
import re
from functools import lru_cache

import tpi

T = None          # type table from tpi.load
BY_UNIQ = {}      # unique name -> defining (non-forward) type index
BY_NAME = {}      # name -> defining type indices


def init(types):
    global T
    T = types
    for i, t in types.items():
        if t['k'] in ('class', 'union', 'enum') and not t['fwd']:
            BY_UNIQ.setdefault(t['uniq'], i)
            BY_NAME.setdefault(t['name'], []).append(i)


def real(i):
    """Follow modifiers and forward references to the defining record."""
    while i is not None and i >= 0x1000:
        t = T.get(i)
        if t is None:
            return i
        if t['k'] == 'mod':
            i = t['to']
            continue
        if t['k'] in ('class', 'union', 'enum') and t['fwd']:
            d = BY_UNIQ.get(t['uniq'])
            if d is None:
                return i
            i = d
            continue
        return i
    return i


def name_of(i):
    i = real(i)
    if i is None:
        return '<none>'
    if i < 0x1000:
        return tpi.PRIM.get(i, hex(i))
    t = T[i]
    return t.get('name') or t['k']


def fields(i):
    t = T[i]
    out, fl = [], t.get('fl')
    while fl is not None:
        items = T[fl]['items']
        fl = None
        for it in items:
            if it[0] == 'cont':
                fl = it[1]
            else:
                out.append(it)
    return out


# --- primitives --------------------------------------------------------------------------------
PRIM_WIN = {
    'void': 0, 'bool': 1, 'char': 1, 'signed char': 1, 'unsigned char': 1, 'char8_t': 1,
    'short': 2, 'unsigned short': 2, 'wchar_t': 2, 'char16_t': 2,
    'int': 4, 'unsigned': 4, 'unsigned int': 4, 'long': 4, 'unsigned long': 4, 'char32_t': 4, 'float': 4,
    '__int64': 8, 'unsigned __int64': 8, 'double': 8, 'long double': 8, 'HRESULT': 4,
    '__int128': 16, 'unsigned __int128': 16, 'std::nullptr_t': 8,
}
PRIM_MAC_CHANGES = {'long': 8, 'unsigned long': 8, 'wchar_t': 4}


def prim(i, mac):
    n = tpi.PRIM.get(i, '')
    if n.endswith('*'):
        return 8, 8, set()
    s = PRIM_WIN.get(n)
    if s is None:
        return 0, 1, {f'unknown primitive {n or hex(i)}'}
    flags = set()
    if mac and n in PRIM_MAC_CHANGES:
        s = PRIM_MAC_CHANGES[n]
        flags.add(f'{n} changes size')
    return s, max(s, 1), flags


# --- standard library ---------------------------------------------------------------------------
# (size, align) of libc++ types on arm64 macOS where the MSVC layout differs or is opaque.
STD_MAC = [
    (r'std::basic_string<', (24, 8)),
    (r'std::basic_string_view<', (16, 8)),
    (r'std::vector<bool,', (24, 8)),
    (r'std::vector<', (24, 8)),
    (r'std::(shared_ptr|weak_ptr)<', (16, 8)),
    (r'std::(map|multimap|set|multiset)<', (24, 8)),
    (r'std::unordered_(map|set|multimap|multiset)<', (40, 8)),
    (r'std::list<', (24, 8)),
    (r'std::forward_list<', (8, 8)),
    (r'std::deque<', (48, 8)),
    (r'std::function<', (32, 8)),
    (r'std::(mutex|recursive_mutex)$', (64, 8)),
    (r'std::timed_mutex$', (120, 8)),
    (r'std::shared_mutex$', (168, 8)),
    (r'std::condition_variable$', (48, 8)),
    (r'std::condition_variable_any$', (64, 8)),
    (r'std::thread$', (8, 8)),
    (r'std::jthread$', (16, 8)),
    (r'std::once_flag$', (8, 8)),
    (r'std::exception_ptr$', (8, 8)),
    (r'std::filesystem::path$', (24, 8)),
    (r'std::locale$', (8, 8)),
    (r'std::any$', (32, 8)),
    (r'std::(stop_source|stop_token)$', (8, 8)),
    (r'std::span<.*,-1>$', (16, 8)),
    (r'std::initializer_list<', (16, 8)),
]
# Laid out from their MSVC members: the same members in the same order in libc++.
STD_RECURSE = re.compile(
    r'std::(pair|optional|_Optional_\w+|array|atomic|_Atomic_\w+|chrono::|complex|reference_wrapper|integral_constant'
    r'|less|greater|equal_to|hash|allocator|default_delete|char_traits|_Variant_\w+|variant|_Tuple_val|span'
    r'|queue|stack|priority_queue|ratio|bool_constant|plus|minus|identity|_Equal_to|_Less|monostate|nullopt_t'
    r'|in_place|piecewise_construct|byte|_Nontrivial_dummy_type|_Non_trivial_\w+|_Deleted_\w+|_Vector_iterator'
    r'|_Vector_const_iterator|_Iterator_base0|_Optional_destruct_base|_Variant_base|_SMF_control\w*)')


def std_rule(name):
    if not name.startswith('std::'):
        return None
    for pat, sa in STD_MAC:
        if re.match(pat, name):
            return ('table', sa)
    m = re.match(r'std::bitset<(\d+)>', name)
    if m:
        n = int(m.group(1))
        return ('table', (max(1, (n + 63) // 64) * 8, 8))
    if name.startswith('std::tuple<'):
        return ('tuple', None)
    if name.startswith('std::unique_ptr<'):
        return ('uptr', None)
    if STD_RECURSE.match(name):
        return ('recurse', None)
    return ('unknown', None)


# --- layout ------------------------------------------------------------------------------------
def up(x, a):
    return (x + a - 1) // a * a


class L:
    """A laid-out type: size, align, dsize (end of data, for tail padding), POD (Apple rule: C++11
    POD), dynamic, members by name -> (offset, type, bit) and base offsets, flags."""
    __slots__ = ('size', 'align', 'dsize', 'pod', 'dynamic', 'members', 'member_flags', 'bases', 'flags', 'empty',
                 'pack')

    def __init__(self, size, align, dsize=None, pod=True, dynamic=False):
        self.size, self.align = size, align
        self.dsize = size if dsize is None else dsize
        self.pod, self.dynamic = pod, dynamic
        self.members, self.member_flags, self.bases, self.flags = {}, {}, [], set()
        self.empty = False
        self.pack = None  # #pragma pack value the Windows layout shows


@lru_cache(maxsize=None)
def win(i):
    """MSVC size and natural alignment as the PDB records them, with an MSVC-model check."""
    i = real(i)
    if i is None:
        return L(0, 1)
    if i < 0x1000:
        s, a, f = prim(i, False)
        lay = L(s, a)
        lay.flags = f
        return lay
    t = T[i]
    k = t['k']
    if k == 'ptr':
        return L(8, 8)
    if k == 'enum':
        return win(t['under'])
    if k == 'array':
        e = win(t['elem'])
        return L(t['bytes'], e.align)
    if k == 'bits':
        return win(t['type'])
    if k not in ('class', 'union'):
        return L(0, 1)
    if t['fwd']:
        lay = L(0, 1)
        lay.flags.add(f'no definition for {t["name"]}')
        return lay
    align, dyn = 1, False
    lay = L(t['size'], 1)
    end = 0
    for it in fields(i):
        if it[0] == 'vf':
            dyn = True
            align = 8
        elif it[0] == 'b':
            b = win(it[1])
            align = max(align, b.align)
            dyn = dyn or b.dynamic
            lay.bases.append((it[1], it[2]))
        elif it[0] in ('vb', 'ivb'):
            dyn = True
            align = 8
        elif it[0] == 'm':
            mt = real(it[2])
            m = win(mt)
            align = max(align, m.align)
            lay.members.setdefault(it[1], (it[3], it[2], None))
    lay.align, lay.dynamic = align, dyn
    # #pragma pack shows as a member or base at an offset its alignment does not divide, or a sizeof
    # that is not a multiple of the alignment. The PDB's "packed" mark alone is not enough: it is set
    # on classes MSVC lays out naturally (SizeLimitedString sits 8-aligned in BlueprintRecord).
    pack = None
    for it in fields(i):
        if it[0] in ('m', 'b'):
            off, a = (it[3], win(it[2]).align) if it[0] == 'm' else (it[2], win(it[1]).align)
            if a > 1 and off % a:
                pack = min(pack or 16, off & -off)
    if t['size'] and t['size'] % align:
        pack = min(pack or 16, t['size'] & -t['size'])
    if pack:
        lay.pack = pack
        lay.align = min(align, pack)
        lay.flags.add(f'{t["name"]} is packed ({pack})')
    return lay


def is_pod11(i, t, flist, lay_bases_pod):
    """Apple's arm64 ABI reuses a base's tail padding unless the base is POD in the C++11 sense:
    trivial and standard-layout. Read off the PDB's flags; an approximation the evidence checks."""
    if 'has ctor / dtor' in t['opts'] or 'overloaded operator=' in t['opts']:
        return False
    access = set()
    for it in flist:
        if it[0] in ('vf', 'vb', 'ivb', 'virt'):
            return False
        if it[0] == 'm':
            access.add(it[4])
    if len(access) > 1:
        return False
    return lay_bases_pod


@lru_cache(maxsize=None)
def mac(i):
    i = real(i)
    if i is None:
        return L(0, 1)
    if i < 0x1000:
        s, a, f = prim(i, True)
        lay = L(s, a)
        lay.flags = set(f)
        return lay
    t = T[i]
    k = t['k']
    if k == 'ptr':
        lay = L(8, 8)
        if t.get('member'):
            lay = L(16 if 'function' in t['mode'] else 8, 8)
            lay.flags.add('pointer to member')
        return lay
    if k == 'enum':
        return mac(t['under'])
    if k == 'array':
        e_win, e = win(t['elem']), mac(t['elem'])
        n = t['bytes'] // e_win.size if e_win.size else 0
        lay = L(n * e.size, e.align)
        lay.flags = set(e.flags)
        lay.pod = e.pod
        return lay
    if k == 'bits':
        return mac(t['type'])
    if k not in ('class', 'union'):
        return L(0, 1)
    name = t['name']
    rule = std_rule(name)
    if rule and rule[0] == 'table':
        s, a = rule[1]
        lay = L(s, a, pod=False)
        return lay
    if rule and rule[0] == 'uptr':
        w = t['size']
        lay = L(8 if w == 8 else 16, 8, pod=False)
        if w not in (8, 16):
            lay.flags.add(f'{name}: unique_ptr of {w} bytes')
        return lay
    if rule and rule[0] == 'unknown':
        lay = layout_class(i, t)
        lay.flags.add(f'standard type without a libc++ rule: {name}')
        return lay
    if rule and rule[0] == 'tuple':
        return layout_tuple(i, t)
    return layout_class(i, t)


def layout_tuple(i, t):
    """MSVC stores tuple<A, B, C> as tuple<B, C> base + A last; libc++ stores A, B, C in order."""
    elems, cur = [], i
    while True:
        fl = fields(cur)
        base = [it for it in fl if it[0] == 'b']
        mem = [it for it in fl if it[0] == 'm' and it[1] == '_Myfirst']
        if mem:
            elems.append(mem[0][2])
        if not base:
            break
        cur = real(base[0][1])
        if not T[cur]['name'].startswith('std::tuple<'):
            break
    lay = L(0, 1, pod=False)
    off = 0
    for e in elems:
        m = mac(e)  # _Tuple_val<T>: T _Val
        off = up(off, m.align)
        lay.flags |= m.flags
        off += m.size
        lay.align = max(lay.align, m.align)
    lay.size = up(max(off, 1), lay.align)
    lay.dsize = off
    return lay


def layout_class(i, t):
    flist = fields(i)
    lay = L(0, 1)
    w = win(i)
    union = t['k'] == 'union'
    bases = [it for it in flist if it[0] == 'b']
    vbases = [it for it in flist if it[0] in ('vb', 'ivb')]
    own_vptr = any(it[0] == 'vf' for it in flist)
    if vbases:
        lay.flags.add(f'{t["name"]} has virtual bases')
    base_lays = [(it, mac(it[1])) for it in bases]
    dynamic = own_vptr or bool(vbases) or any(bl.dynamic for _, bl in base_lays)
    primary = next((bb for bb in base_lays if bb[1].dynamic), None)
    dsize_bits = 0  # data size so far, in bits
    align = 1
    pod_bases = all(bl.pod for _, bl in base_lays)
    flags_so_far = set(lay.flags)
    if dynamic and primary is None:
        dsize_bits, align = 64, 8
    order = ([primary] if primary else []) + [bb for bb in base_lays if bb is not primary]
    # #pragma pack, as the Windows layout shows it: alignment of bases and members capped.
    pack = w.pack or 64
    for it, bl in order:
        flags_so_far |= bl.flags
        if bl.empty and bl is not (primary[1] if primary else None):
            off = 0
        else:
            off = up(dsize_bits // 8 if dsize_bits % 8 == 0 else dsize_bits // 8 + 1, min(bl.align, pack))
            dsize_bits = (off + (bl.size if bl.pod else bl.dsize)) * 8
        align = max(align, min(bl.align, pack))
        lay.bases.append((it[1], off))
        sub = mac(it[1])
        for mname, (moff, mtype, bit) in sub.members.items():
            if mname not in lay.members:
                lay.members[mname] = (off + moff, mtype, bit)
                lay.member_flags[mname] = flags_so_far | sub.member_flags.get(mname, set())
    # Members. Overlapping members (anonymous unions flattened into the field list) are placed
    # where the first member at the same Windows offset went.
    placed = {}  # windows offset -> mac byte offset
    win_end = 0
    bit_unit = None  # (type size bits, unit start bit) of the current run of bitfields
    any_member = False
    for it in flist:
        if it[0] != 'm':
            continue
        any_member = True
        mname, mtype, woff = it[1], it[2], it[3]
        rt = real(mtype)
        is_bits = rt is not None and rt >= 0x1000 and T[rt]['k'] == 'bits'
        m = mac(mtype)
        wm = win(mtype)
        if rt is not None and rt >= 0x1000 and T[rt]['k'] == 'array' and m.flags == {'pointer to member'}:
            # MSVC's member pointer size depends on how it represents the class (4, 8 or 12 bytes),
            # which the dump does not say; a SmallVector's buffer names its element count.
            n = re.search(r',(\d+),\d+>::Wrapper$', t['name'])
            if n:
                e = mac(T[rt]['elem'])
                m = L(int(n.group(1)) * e.size, e.align)
        flags_so_far |= m.flags
        malign = min(m.align, pack)
        align = max(align, malign)
        if union:
            lay.members[mname] = (0, mtype, None)
            lay.member_flags[mname] = set(flags_so_far)
            dsize_bits = max(dsize_bits, m.size * 8)
            continue
        if woff < win_end and not is_bits:
            if woff in placed:
                moff = placed[woff]
            else:
                below = max((o for o in placed if o <= woff), default=None)
                moff = placed[below] + (woff - below) if below is not None else woff
                flags_so_far.add(f'{t["name"]}: overlapping member {mname}')
            lay.members[mname] = (moff, mtype, None)
            lay.member_flags[mname] = set(flags_so_far)
            dsize_bits = max(dsize_bits, (moff + m.size) * 8)
            continue
        if is_bits:
            b = T[rt]
            unit = m.size * 8
            pos = dsize_bits
            if b['n'] and pos // unit != (pos + b['n'] - 1) // unit:
                pos = up(pos, unit)
            byte = pos // 8 // malign * malign  # the unit holding it, at the type's alignment
            lay.members[mname] = (byte, mtype, pos - byte * 8)
            lay.member_flags[mname] = flags_so_far | {'bitfield'}
            dsize_bits = pos + b['n']
            win_end = max(win_end, woff + wm.size)
            continue
        off = up((dsize_bits + 7) // 8, malign)
        placed.setdefault(woff, off)
        lay.members[mname] = (off, mtype, None)
        lay.member_flags[mname] = set(flags_so_far)
        dsize_bits = (off + max(m.size, 0)) * 8
        win_end = max(win_end, woff + wm.size)
    dsize = (dsize_bits + 7) // 8
    lay.align = align
    lay.dynamic = dynamic
    lay.empty = not dynamic and not any_member and all(bl.empty for _, bl in base_lays)
    lay.size = 1 if lay.empty else up(max(dsize, 1), align)
    lay.dsize = 0 if lay.empty else dsize
    lay.pod = (not union or True) and is_pod11(i, t, flist, pod_bases) and all(
        mac(it[2]).pod for it in flist if it[0] == 'm')
    if lay.pod and any(bl.dsize and not bl.empty for _, bl in base_lays) and any_member:
        lay.pod = False  # standard layout allows data members in only one class of the hierarchy
    lay.flags = flags_so_far
    return lay


def find(i, path):
    """Offset of a member path on both platforms: (windows, mac, bit, flags) or None."""
    i = real(i)
    woff = moff = 0
    flags = set()
    bit = None
    for part in split_path(path):
        lw, lm = winmembers(i), mac(i)
        if part not in lw or part not in lm.members:
            # A base class named in the path: its subobject's offset.
            wb = winbase(i, part)
            mb = next((o for b, o in lm.bases if name_of(b) == part), None)
            if wb is None or mb is None:
                return None
            woff += wb[0]
            moff += mb
            flags |= mac(wb[1]).flags
            i = real(wb[1])
            continue
        w = lw[part]
        mo, mtype, b = lm.members[part]
        woff += w[0]
        moff += mo
        flags |= lm.member_flags.get(part, set())
        bit = b
        i = real(mtype)
    return woff, moff, bit, flags


@lru_cache(maxsize=None)
def winmembers(i):
    """Members by name with their Windows offsets, own first, then bases'."""
    i = real(i)
    out = {}
    if i is None or i < 0x1000 or T[i]['k'] not in ('class', 'union'):
        return out
    for it in fields(i):
        if it[0] == 'm':
            out.setdefault(it[1], (it[3], it[2]))
    for it in fields(i):
        if it[0] == 'b':
            for k, (o, ty) in winmembers(it[1]).items():
                out.setdefault(k, (it[2] + o, ty))
    return out


def by_name(name):
    """The defining record of a class by name; the largest when several share it."""
    c = BY_NAME.get(name)
    if not c:
        return None
    return max(c, key=lambda i: T[i].get('size', 0))


def split_path(path):
    """Split at dots outside template brackets: `Dialog<enum A::B>.x` is two parts."""
    parts, depth, cur = [], 0, ''
    for c in path:
        if c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
        if c == '.' and depth == 0:
            parts.append(cur)
            cur = ''
        else:
            cur += c
    parts.append(cur)
    return parts


def winbase(i, name):
    """(offset, type) of the base class `name` of i, searching bases of bases."""
    for it in fields(real(i)):
        if it[0] == 'b':
            if name_of(it[1]) == name:
                return it[2], it[1]
            sub = winbase(it[1], name)
            if sub:
                return it[2] + sub[0], sub[1]
    return None
