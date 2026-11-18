"""Run the Mac port checks end to end: the DLL's names, virtual slots, layouts and calling conventions
against a Windows build (factorio.exe and factorio.pdb) and the Mac build of the same version.

Usage:
  python -I check.py --exe factorio.exe --pdb factorio.pdb --mac <factorio.app>/Contents/MacOS/factorio
                     --queries queries.tsv --work <folder> [--undname undname.exe] [--summary <markdown file>]

queries.tsv is the DLL's own list of what it resolves, with the Windows answers: write it with
`fa_symbols_check <factorio.exe> --queries queries.tsv` (a Windows tool, the build's own).

Needs llvm-nm, llvm-cxxfilt, llvm-pdbutil and llvm-undname on PATH (or undname.exe through
--undname), and the capstone and pefile packages. Each step's output is kept in the work folder as
<step>.log; the summary (GitHub's step summary in CI) gives the numbers. It fails only when a step
fails: the numbers are a report while there is no Mac build of the DLL.
"""
import argparse
import collections
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def run(args, out=None, stdin=None):
    """Run a command; its stdout goes to `out` (a path) or is returned."""
    result = subprocess.run(args, input=stdin, capture_output=True, text=True, encoding='utf-8', errors='replace')
    if result.returncode:
        sys.exit(f'{args[0]} failed with {result.returncode}:\n{result.stderr[-4000:]}')
    if out is None:
        return result.stdout
    with open(out, 'w', encoding='utf-8') as f:
        f.write(result.stdout)
    return None


def script(name, *args, log=None, out=None):
    """Run one of the tools in this folder; returns its stdout, kept in <work>/<log>.log."""
    text = run([sys.executable, '-I', os.path.join(HERE, name), *args])
    if out:
        with open(out, 'w', encoding='utf-8') as f:
            f.write(text)
    if log:
        with open(os.path.join(WORK, f'{log}.log'), 'w', encoding='utf-8') as f:
            f.write(text)
    return text


def demangle(undname, name):
    """An MSVC name in undname's words. undname.exe prints `is :- "<words>"`; llvm-undname prints the
    name back, then the words."""
    text = run([undname, name])
    m = re.search(r'is :- "(.*)"', text)
    if m:
        return m.group(1)
    lines = [line for line in text.splitlines() if line.strip() and line.strip() != name]
    if not lines or lines[-1].startswith('error'):
        sys.exit(f'{undname} cannot demangle {name}: {text}')
    return lines[-1].strip()


def main():
    global WORK
    p = argparse.ArgumentParser()
    p.add_argument('--exe', required=True)
    p.add_argument('--pdb', required=True)
    p.add_argument('--mac', required=True)
    p.add_argument('--queries', required=True)
    p.add_argument('--work', required=True)
    p.add_argument('--undname', default='llvm-undname')
    p.add_argument('--summary')
    a = p.parse_args()
    WORK = os.path.abspath(a.work)
    os.makedirs(WORK, exist_ok=True)
    os.environ['FA_MAC_WORK'] = WORK
    w = lambda name: os.path.join(WORK, name)  # noqa: E731

    print('Mac symbols', flush=True)
    nm = run(['llvm-nm', '--arch=arm64', '--defined-only', a.mac])
    with open(w('mac_nm.txt'), 'w', encoding='utf-8') as f:
        f.write(nm)
    # The two files line up: one demangled name per symbol line.
    raw = '\n'.join(line.split(' ', 2)[2] if line.count(' ') >= 2 else '' for line in nm.splitlines()) + '\n'
    run(['llvm-cxxfilt', '-_'], out=w('mac_dem.txt'), stdin=raw)

    print('Windows types and publics', flush=True)
    run(['llvm-pdbutil', 'dump', '-types', a.pdb], out=w('tpi.txt'))
    run(['llvm-pdbutil', 'dump', '-publics', a.pdb], out=w('win_publics.txt'))

    print('Names and slots', flush=True)
    script('queries.py', a.queries, WORK, log='queries')
    names = json.load(open(w('names.json')))
    with open(w('msvc_dem.tsv'), 'w', encoding='utf-8') as f:
        for _, n in names['addresses']:
            if n.startswith('?'):
                f.write(f'{n}\t{demangle(a.undname, n)}\n')
    script('match.py', WORK, log='match')
    script('slots.py', WORK, a.mac, log='slots')

    print('Layouts', flush=True)
    script('predict.py', log='predict')
    print('Evidence from the Mac code', flush=True)
    script('evidence.py', a.exe, a.mac, log='evidence')
    print('Calling conventions', flush=True)
    script('abi.py', a.mac, log='abi')
    print('Mac symbol table', flush=True)
    script('table.py', a.mac, w('factorio-access-native.symbols'), log='table')

    summary = report(w)
    print(summary)
    if a.summary:
        with open(a.summary, 'a', encoding='utf-8') as f:
            f.write(summary + '\n')


def report(w):
    lines = ['## Mac port check', '']
    match = json.load(open(w('match.json')))
    kinds = collections.Counter(r['kind'] for r in match)
    lines.append(f"- Names: {kinds['ok']} of {len(match)} found once on Mac"
                 + ''.join(f", {v} {k}" for k, v in sorted(kinds.items()) if k != 'ok'))
    for r in match:
        if r['kind'] != 'ok':
            lines.append(f"  - {r['kind']}: {r['name'][:120]}")
    slots = open(w('slots.log'), encoding='utf-8').read()
    m = re.search(r'^Counter\((.*)\)$', slots, re.M)
    first = dict(re.findall(r"'([^']+)': (\d+)", m.group(1))) if m else {}
    # The second pass finds pure virtuals and interface methods in the vtables of subclasses.
    second = re.findall(r'^(pure|interface) (\S+) (\S+) (\{.*\})$', slots.split('-- second pass --')[-1], re.M)
    unresolved = [f'{cls}::{method}' for _, cls, method, votes in second if votes == '{}']
    total = sum(int(v) for v in first.values())
    found = int(first.get('ok', 0)) + len(second) - len(unresolved)
    lines.append(f'- Virtual slots: {found} of {total} found ({len(second) - len(unresolved)} through subclasses)')
    for line in slots.splitlines():
        if line.startswith('OVERLOADED'):
            lines.append(f'  - {line[:140]}')
    lines += [f'  - not found: {name}' for name in unresolved]
    predict = open(w('predict.log'), encoding='utf-8').read()
    m = re.search(r'(\d+)/(\d+) predicted', predict)
    lines.append(f"- Layouts: {m.group(0) if m else 'no result'} of upstream's hand-read 2.1.24 Mac offsets")
    for line in predict.splitlines():
        if line.strip().startswith('MISS'):
            lines.append(f'  - {line.strip()}')
    stats = re.search(r'-- lookups --\n((?:  .*\n)+)', predict)
    if stats:
        lines.append('  - ' + ', '.join(s.strip() for s in stats.group(1).splitlines()))
    evidence = open(w('evidence.log'), encoding='utf-8').read()
    m = re.search(r'^DLL lookups: (.*)$', evidence, re.M)
    lines.append(f"- Mac code against the predicted offsets: {m.group(1) if m else 'no result'}")
    abi = json.load(open(w('abi.json')))
    lines.append(f'- Calling conventions: {len(abi)} functions need a Mac form of their declaration')
    table = open(w('table.log'), encoding='utf-8').read().splitlines()
    lines.append(f'- Mac symbol table: {table[0]}')
    lines += [f'  - {line.strip()}' for line in table[1:]]
    lines.append('')
    lines.append('Logs, data and the Mac symbol table: the work folder (`*.log`, `*.json`, `*.symbols`).')
    return '\n'.join(lines)


if __name__ == '__main__':
    main()
