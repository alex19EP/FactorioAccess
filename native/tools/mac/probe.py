"""Show a class's predicted layout and the methods whose code touches one of its Windows offsets.

Usage: python -I probe.py <factorio.exe> <mac factorio> <class> [windows offset, hex]
"""
import os
import sys

import capstone

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)  # -I leaves the script's folder off the path
import evidence as E  # noqa: E402
import layout  # noqa: E402
import tpi  # noqa: E402

exe, mac_bin, cls = sys.argv[1:4]
woff = int(sys.argv[4], 16) if len(sys.argv) > 4 else None
layout.init(tpi.load(os.path.join(E.WORK, 'tpi.txt'), os.path.join(E.WORK, 'tpi.pkl')))
sys.setrecursionlimit(20000)
i = layout.by_name(cls)
m = layout.mac(i)
print(f'{cls}: windows {layout.T[i]["size"]}, mac {m.size}, flags {sorted(m.flags)[:5]}')
for it in layout.fields(i):
    if it[0] == 'b':
        print(f'  base {layout.name_of(it[1])} win {it[2]:#x} mac {dict((layout.name_of(b), o) for b, o in m.bases).get(layout.name_of(it[1]), "?")}')
for n, (wo, ty) in sorted(layout.winmembers(i).items(), key=lambda x: x[1][0]):
    mo = m.members.get(n, (None, None, None))
    print(f'  {wo:#6x} {mo[0] if mo[0] is None else hex(mo[0]):>7} bit {mo[2]} {n}: {layout.name_of(ty)[:70]} '
          f'win {layout.win(ty).size} mac {layout.mac(ty).size}')
if woff is None:
    sys.exit()
wf, mf = E.win_functions(exe), E.mac_functions(mac_bin)
for k, wl in wf.items():
    if k[0] != cls or k not in mf or len(wl) != 1:
        continue
    wa = sorted({o for o, _ in E.x64_accesses(*wl[0][:2])})
    ma = sorted({o for o, _ in E.a64_accesses(*mf[k][0])})
    if woff not in wa or len(wa) != len(ma):
        continue
    print(f'\n{k}: win {[hex(x) for x in wa]} mac {[hex(x) for x in ma]}')
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    for x in md.disasm(wl[0][1][:160], wl[0][0]):
        print(f'   w {x.address:x} {x.mnemonic} {x.op_str}')
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    for x in md.disasm(mf[k][0][1][:160], mf[k][0][0]):
        print(f'   m {x.address:x} {x.mnemonic} {x.op_str}')
