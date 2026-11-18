"""Predict the Mac offsets and sizes of every lookup in native/src/game.cpp.

Usage: python -I predict.py   (reads work/tpi.txt, work/lookups.json; writes work/predicted.json)
"""
import collections
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)  # -I leaves the script's folder off the path
import layout  # noqa: E402
import tpi  # noqa: E402

WORK = os.environ.get('FA_MAC_WORK') or os.path.join(HERE, 'work')

# Read by hand off the 2.1.24 arm64 build by upstream (native/src/mac/layout.cpp, f2.1 74bbce19).
KNOWN = [
    ('GlobalContext', 'gui', 0x60), ('GlobalContext', 'appManager', 0x168),
    ('agui::Gui', 'baseWidget', 0x70), ('agui::Widget', 'parentWidget', 0x50),
    ('agui::Widget', 'children', 0x58), ('agui::Widget', 'privateChildren', 0x70),
    ('agui::Widget', 'usageBitMask', 0x88), ('agui::Widget', 'toolTipCreator', 0x30),
    ('agui::Button', 'toggled', 0xdd), ('agui::ToggleButton', 'checkedState', 0x124),
    ('agui::Style', 'tooltip', 0x60), ('agui::Style', 'parent', 0x8), ('agui::Style', 'styleInfo', 0x18),
    ('agui::PlainToolTipCreator', 'title', 0x08), ('agui::PlainToolTipCreator', 'text', 0x20),
    ('agui::Table', 'columnCount', 0x284), ('agui::Table', 'hasHeaders', 0x281),
    ('agui::TableWithSelection', 'selectedIndex', 0x288),
    ('agui::Slider', 'value', 0x398), ('agui::Slider', 'valueStep', 0x3b0),
    ('agui::Slider', 'min', 0x3a0), ('agui::Slider', 'max', 0x3a8), ('agui::Slider', 'notchTooltips', 0x180),
    ('agui::MouseEvent', 'position', 0x0), ('agui::MouseEvent', 'button', 0xc),
    ('agui::MouseEvent', 'eventType', 0x10), ('agui::MouseEvent', 'source', 0x28),
    ('agui::MouseEvent', 'previous', 0x30), ('Mod', 'package', 0x0), ('Package', 'packagePath_', 0x40),
    ('SDLWindow', 'window', 0x18), ('Event', 'type', 0x0), ('Event', 'keyboard', 0x10),
    ('KeyboardEvent', 'unichar', 0x08),
]


def main():
    types = tpi.load(os.path.join(WORK, 'tpi.txt'), os.path.join(WORK, 'tpi.pkl'))
    layout.init(types)
    sys.setrecursionlimit(20000)

    print('-- known Mac values (upstream, 2.1.24) --')
    good = 0
    for ty, mem, want in KNOWN:
        i = layout.by_name(ty)
        r = layout.find(i, mem) if i else None
        got = r[1] if r else None
        good += got == want
        if got != want:
            print(f'  MISS {ty}.{mem}: predicted {got if got is None else hex(got)}, Mac {want:#x}, '
                  f'Windows {r[0]:#x}' if r else f'  MISS {ty}.{mem}: not found')
    print(f'  {good}/{len(KNOWN)} predicted')

    look = json.load(open(os.path.join(WORK, 'lookups.json')))
    out = {'offsets': [], 'sizes': []}
    stats = collections.Counter()
    reasons = collections.Counter()
    for field, ty, path in look['offsets']:
        i = layout.by_name(ty)
        r = layout.find(i, path) if i else None
        if r is None:
            stats['not found'] += 1
            out['offsets'].append({'field': field, 'type': ty, 'path': path, 'error': 'not found'})
            continue
        w, m, bit, flags = r
        stats['same' if w == m else 'moved'] += 1
        stats['flagged' if flags else 'clean'] += 1
        for f in flags:
            reasons[f.split(':')[0] if ':' in f else f] += 1
        out['offsets'].append({'field': field, 'type': ty, 'path': path, 'win': w, 'mac': m, 'bit': bit,
                               'flags': sorted(flags)})
    for field, ty in look['sizes']:
        i = layout.by_name(ty)
        if i is None:
            stats['size not found'] += 1
            continue
        w, m = layout.win(i), layout.mac(i)
        stats['size same' if w.size == m.size else 'size changed'] += 1
        out['sizes'].append({'field': field, 'type': ty, 'win': w.size, 'mac': m.size, 'flags': sorted(m.flags)})
    print('-- lookups --')
    for k, v in sorted(stats.items()):
        print(f'  {k}: {v}')
    print('-- why flagged (top) --')
    for k, v in reasons.most_common(25):
        print(f'  {v:5}  {k}')
    json.dump(out, open(os.path.join(WORK, 'predicted.json'), 'w'), indent=1)


main()
