# Xenosaga Episode I
[![Code](https://bytematch.dev/designgears/xsg-i-decomp.svg?label=Code&measure=matched_code_percent)](https://bytematch.dev/designgears/xsg-i-decomp)
[![Data](https://bytematch.dev/designgears/xsg-i-decomp.svg?label=Data&measure=matched_data_percent)](https://bytematch.dev/designgears/xsg-i-decomp)
[![Functions](https://bytematch.dev/designgears/xsg-i-decomp.svg?label=Functions&measure=matched_functions)](https://bytematch.dev/designgears/xsg-i-decomp)

> This project is developed with AI assistance.

A matching decompilation of **Xenosaga Episode I: Der Wille zur Macht**
for the PlayStation 2.

The goal is readable C source that reproduces the original machine code.
The Sony SDK and public libraries do not need to be recovered.

## Progress

<!-- coverage-report:begin -->
Recovered 7,550 of 7,645 in-scope game function(s) (98.757%).

- `exact_c` (pure C): 7,127 function(s)
- `exact_c_with_asm`: 418 function(s)
- `exact_asm`: 5 function(s)
- `exact_vu_microcode`: no accepted VU microprogram
- empty-body functions: 0 of the counted function(s) are 8-byte `jr ra; nop` bodies
- These counts come from the tracked object tables, the tracked symbol
  tables and the published source. This tree cannot check that each
  counted function matches its accepted record under its recorded
  contract, independently reviewed: config/units and config/vu-build.json are not tracked.
<!-- coverage-report:end -->

Progress is tracked on [ByteMatch](https://bytematch.dev/designgears/xsg-i-decomp) from the objdiff report (badges above).
The counts below are source-level: functions whose C (or accepted asm) is in the tree.

| Version | Target | Functions | Progress |
| --- | --- | ---: | ---: |
| NTSC-U | `SLUS_204.69` | 3,620 / 3,671 | 98.611% |
| NTSC-U | `OV01.OVL` | 1,055 / 1,081 | 97.595% |
| NTSC-U | `OV02.OVL` | 109 / 110 | 99.091% |
| NTSC-U | `OV10.OVL` | 358 / 361 | 99.169% |
| NTSC-U | `OV11.OVL` | 137 / 139 | 98.561% |
| NTSC-U | `OV12.OVL` | 1,776 / 1,788 | 99.329% |
| NTSC-U | `SSD.IRX` | 442 / 442 | 100.000% |
| NTSC-U | `RSSD.IRX` | 53 / 53 | 100.000% |

Full per-unit progress and history: [bytematch.dev/designgears/xsg-i-decomp](https://bytematch.dev/designgears/xsg-i-decomp)

## Building

The historical compilers are 32-bit Linux executables. On 64-bit Ubuntu,
install `libc6-i386` and `python3-venv`. On Windows, use WSL and keep the
checkout and build on a Linux filesystem: the old compilers cannot read some
Windows filesystem metadata and report `Value too large for defined data type`.

You need three things the repository does not contain: your own copy of the
game, the original toolchain, and a Python environment for the disassembler.

### 1. The originals

Extract `SLUS_204.69`, `OV01.OVL`, `OV02.OVL`, `OV10.OVL`, `OV11.OVL`,
`OV12.OVL` and the `IOP/` directory from your own disc image and put them in one
folder. `config/originals.json` names the six files with their sizes and
SHA-256; the build refuses anything else, so it tells you immediately if you
have a different release. This project reproduces the NTSC-U release.

### 2. The toolchain

`config/toolchain-identity.json`: for every compiler and
assembler it gives the version, the SHA-256 of each payload, and the provenance
needed to obtain or rebuild it. Two of the tools are patched, and both patches
are recorded there in full:

| Tool | What the build uses it for | Where it comes from |
| --- | --- | --- |
| `ee-gcc2.96-realconv-lp7` | `cc1` for the game code (Monolith) and the C runtime (newlib/libgcc) | the published `ee-gcc2.96` archive, with eighteen documented patches to `cc1` (six single bytes that restore the original decimal-literal rounding, two that restore the R5900 short-loop padding, four multi-byte alias-analysis patches, fsv1 and fsv3: `%lo`-addressed global scalars and struct-field accesses may be reordered in either direction, and a non-small-data global's memory reference gets its type's alias set; fsv4: gcse load-PRE transparency uses `rtx_varies_p`; two fsv5 patches: a char widened to 64 bits expands as `extendqisi2` + `extendsidi2`; plus the documented usecount and externname fixes) |
| `ee-gcc2.9-991111` | `cc1` for the SCE SDK libraries (Sony's own compiler; out of recovery scope, built from original assembly) | the published `ee-gcc2.9-991111` archive, unmodified |
| `ee-as-la29-vsqrt` | assembler for every game and runtime object | GNU as 2.9-ee-991111 built from the pinned `ps2-ee-toolchain` commit with the recorded five-change diff (an overlap-safe `memmove`, the `vsqrt` opcode's bits 21-22, labels left before the hazard nops `append_insn` inserts, `la` of a base register plus a 16-bit constant as one `addiu`, and an optional `-mswap-fix` flag, off by default, that lets `append_insn` fill delay slots after macro expansions; passed only to the TUs in `config/tu/as-flags.json`) |
| `ee-as-2.9-plain` | assembler for the SCE SDK objects | the same commit, unpatched |
| `ps2dev-binutils` | linker and objcopy of every image, and modern GAS for data objects and retail blobs | ps2dev binutils 2.45.1, the linker, objcopy and modern GAS |

Lay them out under one directory as `config/toolchain-identity.json`'s
`tools.<id>.dir` / `.path` say. Then:

```bash
python3 -B tools/tu/toolchain.py --check
```

It prints where it looked and what it found, and refuses with the tool's name,
both hashes and the recipe when something is missing or different.

### 3. The disassembler

```bash
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
```

### 4. Tell the build where those three are

By environment variable:

```bash
export XENO_GAME_DIR=/path/to/extracted-iso
export XENO_TOOLCHAIN_DIR=/path/to/toolchains
export XENO_SPLAT_PYTHON=$PWD/.venv/bin/python
```

or once, in `config/toolchain-local.json` (untracked):

```json
{
  "game_dir": "/path/to/extracted-iso",
  "toolchain_root": "/path/to/toolchains",
  "splat_python": "/path/to/.venv/bin/python"
}
```

Three of the tools are built from source rather than downloaded, so their bytes
depend on the host compiler. A build of the same source that assembles the same
bytes still reproduces the images: accept it one tool at a time with
`XENO_TOOLCHAIN_ALLOW=ee-as-la29-vsqrt,ee-as-2.9-plain,ps2dev-binutils`. The
build then says which check it relaxed.

### 5. Build

```bash
./configure.py      # generate every unit build directory and build.ninja
ninja               # build and compare all six originals
ninja gate          # + per-unit status and the whole-file SHA-256 gate
```

`ninja gate` writes `build/gate.json`. `"result": "pass"` means every one of the
six files came out byte for byte identical to your original.

## Contributing

Pick an object from `config/objects/<unit>.objects.json` that has no source yet,
write the C, and build: `ninja` tells you whether the file still comes out
identical. A function counts when its translation unit's source defines it in C
and the whole-file gate passes.

## Acknowledgments

Thanks to the developers and contributors of the projects on which this work relies:

- [OpokXeno/xsg-i-decomp](https://github.com/OpokXeno/xsg-i-decomp), the upstream project this
  repository builds on, and its contributors (including TreeThugga, LinuxJessi and
  derwetterfrosch), whose matched functions and recovered types are ported here.
- [splat](https://github.com/ethteck/splat),
  [spimdisasm](https://github.com/Decompollaborate/spimdisasm), and
  [Rabbitizer](https://github.com/Decompollaborate/rabbitizer) for splitting and disassembly.
- [m2c](https://github.com/matt-kempster/m2c) for first-pass decompilation of MIPS
  assembly into C.
- [ps2dev/binutils-gdb](https://github.com/ps2dev/binutils-gdb) and
  [SSXModding/ps2-ee-toolchain](https://github.com/SSXModding/ps2-ee-toolchain) for EE toolchains.
- [decomp.me's compiler collection](https://github.com/decompme/compilers) for historical compiler distributions.
- [PCSX2](https://github.com/PCSX2/pcsx2) for its documented EE implementation and behavioral reference.

## Disclaimer

A legally obtained copy of the game is required. No original game binaries or
copyrighted game assets are distributed in this repository. All game rights belong
to their respective owners; this project is not affiliated with or endorsed by them.
