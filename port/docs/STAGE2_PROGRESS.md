# PC port, Stage 2: every game TU compiles for the host

Branch `pc-port`, 2026-11-06, `gcc (GCC) 16`, i386 (`-m32`).

**Result: 347 / 347 game C TUs compile** (`src/main` 184, `ov01` 37, `ov02` 15, `ov10` 11,
`ov11` 3, `ov12` 96, plus `src/main/nml_packet_add/nml_packet_trans_microcode.c`) into
`libxsg_game.a`, with inline asm stubbed. Nothing is linked yet.
The PS2 matching build is unchanged: `ninja gate` passes for all 6 units, and every PS2
game object (`build/main/build/c/*.o`, `build/ov*/build/scaffold/src/**/*.o`, 428 files) is
**byte-identical** (sha256) to the objects built from `fa04046`, the commit before Stage 2.

## Build

```sh
cmake -S port -B port/build && cmake --build port/build -j
# -> port/build/libxsg_game.a (347 objects), per-unit object libraries xsg_<unit>
cmake -S port -B port/build -DPORT_ASM_TRAP=ON   # stubbed asm executes __builtin_trap()
cmake -S port -B port/build -DPORT_WERROR_C=OFF  # silence the downgraded warnings (-w)
```

`port/CMakeLists.txt` compiles the shared `src/` directly. The flags are:
`-m32 -std=gnu89 -fno-strict-aliasing -fwrapv -ffp-contract=off -fsigned-char -fno-common
-fgnu89-inline -fno-builtin -fno-pic -fno-stack-protector -fno-delete-null-pointer-checks
-DPORT_HOST -DTI=DI -include port/include/ps2_host_compat.h -I include -I .`
(the same `-Iinclude -I.` contract as the PS2 build, from the repo root). They also include
`-Wno-error=` for implicit-function-declaration, implicit-int, int-conversion,
incompatible-pointer-types, return-mismatch and declaration-missing-parameter-type.

A clean build currently prints 1094 warnings: incompatible-pointer-types 520,
stringop-overread 156, int-conversion 134, overflow 42, stringop-overflow 20,
discarded-qualifiers 17, shift-count-overflow 15, trigraphs 6 and varargs 1.

## How inline asm is handled (and why)

**Mechanism: the asm statements are wrapped in macros in the shared source.** The new header
`include/port/ps2_asm.h` defines three macros:

| macro | PS2 build (no `PORT_HOST`) | host (`-DPORT_HOST`) |
|---|---|---|
| `PS2_ASMV(...)` | `__asm__ __volatile__(...)` | `((void)0)`, or `__builtin_trap()` with `PORT_ASM_TRAP` |
| `PS2_ASM(...)` | `__asm__(...)` | same stub |
| `PS2_REG("$f21")` (in `register T x PS2_REG(...)`) | `__asm__("$f21")` | nothing (the binding only steered ee-gcc register allocation) |

The macros use GNU `args...` variadics, not `__VA_ARGS__`, because the
`ee-gcc2.9-991111` compiler variant's cpp has no `__VA_ARGS__`. Both PS2 compilers were
checked. File-scope ELF alias directives (`__asm__(".equ x.11, x.0")`, `.size`, `a = b`;
9 of them in 5 files) are wrapped in `#ifndef PORT_HOST` instead. Asm *labels*
(`extern T x[] __asm__("sym")`, `NMLE_SYM`) are left alone, because host gcc accepts them.

`port/scripts/wrap_inline_asm.py` made the rewrite: 1073 sites in 85 files, adding
`#include "port/ps2_asm.h"` at the top of each. It is idempotent, so run it again after
importing new code. `--check` exits 1 if anything is still unwrapped (it reports 0 now).

Why this approach:
* A macro can't remove `__asm__ __volatile__(...)` from outside, and a generated host copy of
  the tree (the Stage 1 survey's stubtree) would drift from the sources.
* With the wrapper there is a single source of truth. The host build reads `src/` directly,
  and each asm block now has a grep-able name (`PS2_ASMV`) to replace one by one in Stage 3.
* The PS2 expansion is token-identical to the original, which the byte-identical objects
  confirm. ee-gcc never sees a different token stream, so matching is not at risk.
* For Stage 3, a block is ported by wrapping it as
  `#ifndef PORT_HOST PS2_ASMV(...) #else <C or VU0-emu> #endif`, or with a helper macro that
  has a PS2 and a host expansion.

## Source changes to src/ / include/ (all matching-neutral, verified)

Commits on `pc-port`:

1. `1527d32` **port: route inline asm through include/port/ps2_asm.h macros.** Adds the
   header and the script, and rewrites the 85 files.
2. `cf098d6` **src: make prototypes agree with static definitions** (the 67
   static-follows-non-static errors):
   * 46 same-file `extern`/plain prototypes become `static`;
   * `act_2.h` had two prototypes of static functions in `act_2.c`; they move into
     `act_2.c` as `static`;
   * the TU-local headers `ov12/xrg_event.h`, `xrg_sound.h` and `xrg_sysinit.h` lose their
     `extern` declarations of file-static data (only their own `.c` includes them, and every
     use comes after the definition);
   * implicit calls before a static definition (`sef.c` ×5, `set_path.c` ×1) get `static`
     prototypes;
   * `sef.c`: `static static` becomes `static` (the duplicate-static error).
3. `541c933` **src: resolve remaining host-gcc declaration conflicts** (the 34
   conflicting-types/redeclared errors, 3 cast-lvalues and 1 incomplete type):
   * **Per-scope type views of one symbol**: block-scope `extern`s that re-type a global
     differently in different functions now get a unique C name bound by asm label to the
     same symbol. Cases: `game.c` `actor` ×2, `matrix_print.c` `acos`/`fptodp` (u64 bit
     views), `sc_get.c` `tracePrint` (3-arg view), and in `nml_model_set.c` `s_pMapLast`,
     `s_aNonAlphaGroupView`/`s_aAlphaGroupView`/`sRenderView` (`…Up`/`…Rp`).
   * **Implicit call, later typed declaration**: the existing declaration is copied above the
     function with the first use: `game.c`, `menu_1.c`, `menu_ether.c`,
     `sv_get_image_item.c` ×3, `xgl_1.c`, `ov01/battle_init.c` ×3, `calc.c` ×3,
     `data_unit_org_get.c`, `menu.c` ×2, `snd.c` ×2, `ov12/rg_announce.c`,
     `rg_battlemgr.c`. `menu_1.c` `UnitOrgWeapons` gets a struct tag so the hoisted
     prototype can name it.
   * `ov01/calc.c` `calcStatClr`: the unused parameter `val`, shadowed by a local, is renamed
     `val_param`.
   * `ov12/rg_robot.c`: the unprototyped re-declarations `static int _DashPassTime();` and
     `_DashVRPassTime();` are dropped. Their definitions above are prototyped, and the only
     later use takes the address.
   * `call_java_method.h`: `extern EnemyWork enepc[16]` with an incomplete element type
     becomes `extern unsigned char enepc[]` (every use casts it).
   * **`PORT_HOST`-guarded**, because the portable spelling changed ee-gcc output (the gate
     caught it):
     * `sef.c` `_srsMemRes` view: an asm-label alias changes gp-relative addressing;
     * `xgl_cd.c` `*((int *)p)++` becomes `*(*(int **)&p)++`, since taking the address
       forces the local into memory;
     * `seisan_count_init1.c` `#define SeisanResult ((PortResult *)SeisanResult)` becomes a
       deref form on the host only.
4. **port: CMake host build skeleton and Stage 2 progress doc** (this doc, plus
   `port/CMakeLists.txt`).

## Per-file status

| unit | TUs | compile on host |
|---|---|---|
| main | 184 | 184 |
| ov01 | 37 | 37 |
| ov02 | 15 | 15 |
| ov10 | 11 | 11 |
| ov11 | 3 | 3 |
| ov12 | 96 | 96 |
| **total** | **347** | **347** |

81 source files (`.c`/`.h`) still hold stubbed asm sites; see "What's left".

Every TU builds to an object. The per-TU details (diagnostics and warnings) are reproducible
with `cmake --build port/build -- -k 0 2>&1 | grep -E 'error|warning'`.

## What's left

* **Inline asm to replace: 1039 statement sites** (994 `PS2_ASMV`, 45 `PS2_ASM`) in 81
  files, plus 26 `PS2_REG` bindings (already no-ops on host) and 9 `#ifndef PORT_HOST`
  file-scope alias directives (they need host equivalents only if the aliased local-static
  names are referenced). Some sites are in macros (`Vu0SubVector`, `QuadwordCopy`,
  `ACT_IM_ZERO_QW`...), so fixing one macro covers many expansions. Top files: sef.c 161,
  nml_model_set.c 72, xgl_2.c 64, m_math.c 59, ov01/m_cam.c 33,
  ov01/m_ef_create_kosbw_02.c 33, ov01/m_ef_create_smp_01.c 32, ov12/rg_geom_ball.c 30,
  sv_get_image_item.c 29, face_point.c 28, xgl_camera.c 28. Stage 1 has the breakdown by
  instruction kind (VU0 macro mode 3754 instructions, lq/sq 904, scalar 347, MMI 42).
* **128-bit `mode(TI)`**: 44 typedefs. The CMake build passes `-DTI=DI`, so on the host these
  quadword types are **8 bytes, not 16**. That keeps them compiling but breaks struct layout
  and any 16-byte copies. Next step: change those typedefs to `ps2_u128` under `PORT_HOST`.
* **Semantics that compile but are wrong on the host** (to fix before linking or running):
  * the soft-float bit views in `matrix_print.c` (`acos_bits`/`fptodp_bits` alias the libm
    and libgcc names with u64 signatures). The port's libps2 must provide `fptodp`/`dptofp`/
    `dpmul`/`litodp`/`dptoli` and route these calls correctly;
  * `enepc` is now an untyped extern;
  * the 520 incompatible-pointer and 134 int-conversion warnings.
* **Link**: the platform/SDK stubs (about 160 symbols, Stage 1 §Platform API surface), the
  overlay `func_A19xxx` absolute references, `_gp` and `__builtin_args_info` varargs.
* `port/scripts/host_compile_survey.py` still uses its Stage 1 stubtree. It is superseded by
  the CMake build for compile status and is kept for its static inventories.
