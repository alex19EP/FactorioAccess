# Mac port checks

Offline checks of whether FactorioAccess's native DLL can resolve its names, virtual slots and
field layouts on the Mac arm64 build, and where its calls into the game differ. Inputs: a Windows
build's `factorio.exe` and `factorio.pdb`, and the Mac build of the same version (the binary in
`factorio.app/Contents/MacOS`, from the .dmg). Run every script with `python -I`; they need
`capstone` and `pefile`, and LLVM's `llvm-nm`, `llvm-cxxfilt`, `llvm-pdbutil` and `llvm-undname`
(on Windows, MSYS2's `mingw-w64-ucrt-x86_64-llvm-tools`).

`check.py` runs every step below, writes the Mac symbol table and prints a summary:

```
fa_symbols_check <factorio.exe> --queries <folder>/queries.tsv
python -I native/tools/mac/check.py --exe <factorio.exe> --pdb <factorio.pdb> \
   --mac <factorio.app>/Contents/MacOS/factorio --queries <folder>/queries.tsv --work <folder>
```

`queries.tsv` is the DLL's own list of everything it resolves, with the Windows answers, written by
the Windows symbol checker (`native/tools/check_symbols.cpp`). It includes the queries built at run
time (frame types, enum arrays, prototype maps), which no scan of the source finds.

The game check workflow (`.github/workflows/game-check.yaml`, job `mac-check`) runs it on every new
release, on Linux, with the query list from the `symbols` job and the summary on the run's page. The steps below run one by one with the
folder in `FA_MAC_WORK` (default `work/` here, which git ignores).

## Step 1: names and virtual slots

1. `llvm-nm --arch=arm64 --defined-only <factorio> > work/mac_nm.txt`, then
   `cut -d' ' -f3- work/mac_nm.txt | llvm-cxxfilt -_ > work/mac_dem.txt` (the two files line up).
2. `queries.py <queries.tsv> work`: the names and slots into `work/names.json`, the offsets, sizes
   and enumerators into `work/lookups.json`.
3. Demangle the MSVC names with `llvm-undname` (or `undname.exe`) into `work/msvc_dem.tsv`
   (`name<TAB>demangled`).
4. `match.py work`: each name against the Mac symbols; `slots.py work <factorio>`: each virtual
   slot in the Mac vtables (plain absolute pointers; pure virtuals resolved through subclasses).

## Step 2: layouts

1. `llvm-pdbutil dump -types factorio.pdb > work/tpi.txt` (600 MB, seconds).
2. `predict.py`: Mac offsets and sizes of every lookup into `work/predicted.json`, checked first
   against upstream's 34 offsets read by hand off the 2.1.24 arm64 build.

`layout.py` holds the rules: libc++ sizes of standard types, `long`/`wchar_t`, Itanium base and
vptr placement, tail padding reused unless the base is a C++11 POD (Apple arm64), Itanium
bitfields, packing (marked, or shown by a sizeof that is not a multiple of the alignment), MSVC
tuples stored backwards, member-pointer buffers. Each prediction carries flags for what the rules
could not see.

## Step 3: evidence from the Mac code

1. `llvm-pdbutil dump -publics factorio.pdb > work/win_publics.txt`.
2. `evidence.py <factorio.exe> <mac factorio>`: for each non-template instance method present once
   by name on both platforms (small ones, up to 768 bytes), the offsets its code touches through
   `this` (rcx / x0, following copies, lea and add-immediates). Member order is the same on both, so
   equal counts pair up sorted. Each pair is checked against the rules; the DLL's lookups are checked
   against the pairs. Writes `work/evidence.json`.
3. `probe.py <exe> <mac> <class> [win offset]`: a class's layout on both platforms and the paired
   methods' disassembly, to look into a contradiction.

Things that look like errors and are not: a std::map read at +8 on Mac (libc++ root pointer) and
+0 on Windows (`_Myhead`); MSVC passes an override of a secondary base's virtual a `this` pointing at
that base, clang a full-object `this` (such methods are skipped); long functions cut at the window.

## Step 3b: calling conventions

`abi.py <mac factorio>`: for each game function the DLL resolves, where a class travels by value
and the register shape differs (MSVC: member functions return classes through a hidden pointer in
rdx, free ones in rcx unless 1/2/4/8-byte POD; by-value classes over 8 bytes go as a pointer to a
copy. arm64: trivially copyable classes up to 16 bytes in x0/x1 or v0-v3, others through x8 or a
pointer). Returns are settled from the Mac code (x8 read before written). Writes `work/abi.json`.
Virtual slots: their names from `win_publics.txt` into `work/slot_mangled.txt`, demangled with
`llvm-undname` into `work/slot_dem.tsv`, classified with the same functions.

## Step 4: the Mac symbol table

`table.py <mac factorio> <out>` writes `factorio-access-native.symbols` in the format of the DLL's
symbol cache, keyed by the Windows queries so `game.cpp`'s `resolve` runs unchanged on the Mac:

```
mac rules 2 builds <LC_UUID> [<LC_UUID> ...]
name <decorated name>\t<Mac symbol name>
off <type> <path>\t<offset>
size <type>\t<size>
enum <type> <enumerator>\t<value>
vslot <type> <method>\t<index from the vtable pointer>
```

Functions and globals are named, not addressed: the shipped Mac binary keeps its symbol table, so
the DLL finds them at start and they survive a game update. Only the layout entries depend on the
build, and a table holds for every build in its header: a release whose layouts did not change is
added to the list instead of getting a new table. A build not listed makes the DLL stand down. A path
into MSVC's standard library (`queue._Mypair._Myval2._Map`) becomes the container's own offset,
read on the Mac by the libc++ form of `stdabi`. Queries with no Mac answer are left out and listed.
