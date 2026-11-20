# PC port, Stage 7: host VU0 (macro + micro mode) and the game's inline asm

> **Settings (2026-11-07):** the port no longer reads `XSG_*` environment variables. Every
> `XSG_FOO=v` in this document is now `[section] key = v` in settings.ini (the mapping is the
> "replaces" column of [MENU.md, settings.ini reference](MENU.md#settingsini-reference)); run
> with `XSG_CONFIG=<file> port/build/xsg`. Samples: [examples/](examples/).

Branch `pc-port`, 2026-11-07. Builds on [STAGE6_VU1.md](STAGE6_VU1.md).

**Result:** 1038 of the 1039 stubbed inline-asm statements (81 files) now run as host C
on a VU0/EE register model, and `vcallms`/`vcallmsr` run the game's resident VU0
microprogram. The PS2 path is unchanged (`ninja gate`: pass). The unit tests pass
(116 checks). In-game, the 3D renderer now feeds the VU1 geometry microcode. Before this
stage VU1 drew 0 primitives after New Game. Now it draws about 5,400–6,000 triangle
strips per frame (frames 390–570: ~60k–150k VU1 runs and 30–72 M instruction pairs per 30
frames, with clip/status flag reads). The run reaches the 30 s timeout (frame 570+) with
no fault. Combined with the other agents' parallel fixes, it no longer stops at ~372.
The frames are still black: every triangle lands on one screen point (see "Next").

## 1. Design

### Register model: `port/runtime/vu0.{h,c}`

* `xsg_vu0`: VF0–VF31 (VF0 is forced back to 0,0,0,1 after every write), ACC, VI0–VI15,
  I, Q, P, R, status/MAC/clip, CMSAR0, FBRST.
* `xsg_ee_gpr[32]` (128-bit), `xsg_ee_fpr[32]`, `xsg_ee_fcc`: the EE registers that asm
  templates name directly (`$8`, `$f1`, …). `$0` reads `xsg_ee_zero`; writes to it go to
  `xsg_ee_sink`.
* All of this is **global and persistent**, as on hardware. One block loads vf28..vf31 or
  a GPR, and a later block or microprogram uses it.
* Macro ops:
  * `vu0_arith(op, dest, fd|VU0_ACC, fs, ft, bc)` covers ADD/SUB/MUL/MADD/MSUB/MAX/MINI.
    `bc` is the vector form, lanes x..w, Q or I. The ACC forms take `fd = VU0_ACC`.
  * `vu0_opmula`/`vu0_opmsub`, `vu0_move`/`mr32`/`abs`, `vu0_itof`/`ftoi` (0/4/12/15),
    `vu0_clip`.
  * `vu0_div`/`sqrt`/`rsqrt`: Q is written at once, so `vu0_waitq` is a no-op.
  * `vu0_mfir`/`mtir`/`iadd`/`isub`/`iaddi`/`iand`/`ior`, `lqi`/`sqi`/`lqd`/`sqd`/`ilwr`/`iswr`
    (VU0 data memory), `rinit`/`rget`/`rnext`/`rxor`.
  * `lqc2`/`sqc2`/`qmtc2`/`qmfc2`/`ctc2`/`cfc2`. `cfc2`/`ctc2` support VI1–15 and control
    regs 16/17/18/20/21/22/27/28.
  * MMI: `xsg_mmi_*` (pcpyld pcpyud pextlw pextuw pexew pxor por pand pnor paddw psubw
    pcgth pextlh pextlb pmaxw pminw ppach ppacb pcpyh paddh psubh).
* Floats (vu0.c is built `-msse2 -mfpmath=sse`): handled as in vu1.cpp. Inputs with
  exponent 255 read as ±FLT_MAX and denormals as ±0. Results clamp to ±FLT_MAX (NaN
  becomes +MAX) and set MAC/status. MADD/MSUB round the product first. Division by 0
  gives ±MAX and sets the D/I flags.
* **Not bit-exact:** the host rounds to nearest, while the VU truncates.

### Micro mode (vcallms / vcallmsr)

The game DMAs `Vu0MicroCode` (224 pairs, `src/main/vu0/Vu0MicroCode.dvp`) to VU0 once over
VIF0, which the host does not run. `port/scripts/gen_vu0_microcode.py` takes the MPG
payload from the PS2 build's `build/main/build/vu0/Vu0MicroCode.o` (`.vudata`: size
qword, DMAcnt, VIF NOP, MPG 224@0). CMake compiles it into `vu0_microcode.c`. The first
`vcallms` copies it to VU0 micro memory (0x11000000).

`xsg_vu1_run_vu0(pc)` in vu1.cpp runs the program on the **VU1 interpreter** with VU0's
4 KB memories (base/mask globals) and the `xsg_vu0` register file. It saves and restores
the VU1 state and statistics around the call. `vcallms 0x20`/`0xE8` = Vu0CallSin/Cos
(81 sites). `vcallmsr $vi27` (30 sites) runs at CMSAR0×8: the 18 matrix-stack entries
(xglMatrixStack*, xglLightCalcMatrix).

**vu1.cpp fix found here:** a lower-slot WAITQ/WAITP stalls its whole pair, so the
upper op of that pair must already see the new Q/P (`mulq.xyz vf28,vf03,q | waitq` in
Vu0MatrixStackInverse). The interpreter ran the upper op first, so it read a stale Q.
Now it lands Q/P first. This also applies to VU1 programs (5 WAITQ, 3 WAITP there).

### Translation: `port/scripts/vu0_translate.py`

Each `PS2_ASMV(...)`/`PS2_ASM(...)` is parsed: string-literal template, outputs, inputs.
If every instruction is understood, it is rewritten as

```c
#ifndef PORT_HOST
    PS2_ASMV(...original...);
#else
    {
        XsgGpr r_1 = XSG_IN(matrix);          /* operands */
        vu0_lqc2(1, XSG_EA(r_1, 0));
        vu0_arith(VU0_MUL, 15, VU0_ACC, 2, 1, 0);   /* vmulax.xyzw ACC, vf2, vf1x */
        ...
        XSG_OUT(result, r_0);
    }
#endif
```

* **Operands:**
  * `"r"` becomes an `XsgGpr` (raw bits, with 32-bit values sign-extended, as in an EE
    register). `"f"` becomes an `XsgFpr`. `"m"` becomes an address.
  * `+`/`=`/`=&` operands and matching (`"0"`) constraints are handled.
  * A variable declared `register T x PS2_REG("$N")` in the same function is bound to
    `xsg_ee_gpr[N]` / `xsg_ee_fpr[N]`. This matters, e.g. in MMathCalcHermite: the `=r`
    and `r` operands share $10.
* **Symbols:** symbols named in templates (`%hi/%lo(sym)`, `lw $8,sym`) are bound by
  assembler name: `extern char xsg_sym_X[] __asm__("X")`. That works for file statics
  and for undeclared data.
* **EE subset:**
  * addi(u)/add(u)/sub(u)/dadd(u)/dsub(u)/daddiu, move, li, lui, and/or/xor/nor(+i),
    slt(i)(u), shifts incl. the 64-bit ones, movz/movn;
  * lb..ld/lq/ldl/ldr/lwc1, sb..sq/swc1, mfc1/mtc1, li.s, add/sub/mul/div/mov/neg/abs/
    sqrt.s, cvt.w.s/cvt.s.w, c.lt/le/eq.s;
  * branches (b/j/beq/bne/beqz/bnez/bgez/bltz/blez/bgtz/bc1t/bc1f) to `goto` labels,
    with numeric `1f`/`1b` labels resolved;
  * under `.set noreorder` the delay slot runs before the jump;
  * nop/sync/ei/break/.align are dropped.
* An asm statement inside a `#define` gets the whole define duplicated under the
  `#ifndef`, with the host body as a statement expression.
* The script is idempotent: a site already under `#ifndef PORT_HOST` is skipped.
  `--report` lists the untranslated sites, `--dry` changes nothing.
* Every translated block increments `xsg_vu0_blocks`. `XSG_VU0_STATS=1` prints it and
  the vcallms count at a clean exit.

lq/sq/lqc2/sqc2 use the address as given. They do not mask the low 4 bits as the EE does,
because host structures might not be 16-byte aligned.

## 2. Coverage

| | sites |
|---|---|
| total statements (`PS2_ASMV` + `PS2_ASM`) | 1039 in 81 files |
| translated | **1038** |
| priority files | nml_model_set.c 72/72, xgl_2.c 64/64, m_math.c 59/59, face_point.c 28/28, nml_packet_add.c 15/15, nml_filter_set.c 13/13 |
| untranslated | **1**: `src/ov01/m_cam.c` (`#addr` stringised into the template inside a macro; hand-port when ov01 cameras matter) |

The 26 `PS2_REG` bindings stay no-ops for codegen, but the translator uses them to bind
operands to the right hard register.

Not modelled:
* VU0 micro-mode MAC/clip latency differences between macro and micro calls;
* `sw $31,%0` (Undulate.c) stores the model's `$31`, not the real return address;
* `$gp`/`$sp` are rejected (none occur).

## 3. Tests: `port/tests/test_vu0.c`

`cmake --build port/build-vu0 --target test_vu0 && port/build-vu0/test_vu0` gives
**116 checks, 0 failures**. It links the real `m_math.c` (translated) with vu0.c, vu1.cpp
and the generated microcode, and maps VU memory at 0x11000000. It covers:

* the macro ops: VF0 constancy, dest masks, bc/ACC chains, FLT_MAX clamping (no inf),
  div/sqrt/rsqrt and divide by zero through Q, opmula/opmsub (cross product), ftoi4/itof4,
  mr32, clip flags through cfc2 $vi18, viaddi/vmfir;
* MMI pcpyld/pextlw/pcpyud;
* m_math: MMathMulMatrix (= right × left, row vectors), ApplyMatrix, CalcLength,
  CalcDist, Dot/Cross product, NormalizeVector2, RotateMatrixX/Z (through vcallms
  sin/cos), Translate, Scale (row i × s[i]);
* microcode: Vu0CallSin/Cos over −3..3 (within 5e-5), MatrixStackMul,
  MatrixStackInverse (M·M⁻¹ = I within 1e-4; this needed the WAITQ fix), PushUnit/Pop.

There is no PS2-trace comparison yet.

## 4. Game run

`XSG_PAD=300:0x0800 XSG_MC_DIR=/tmp/xsgmc-vu0 XSG_TIMEOUT=30 port/build-vu0/xsg`:

* **Crash fixed:** with `_CopyMatrix` live, `SGsRestoreEnv` faulted at frame 302 on
  address 0x43000000. `nmlModelSetLight(_defMatLcMod, _defMatLn)` passes two 64-byte
  structs to an implicitly declared function. EE EABI passes them by reference; i386
  pushes the floats. The fix is host-only (`#ifdef PORT_HOST` passes `&`), in commit
  9ff9a4e. `check_call_types.py` does not flag struct-by-value through implicit
  declarations, so other such calls may exist.
* **VU1 stats (`XSG_VU_LOG=30`):**
  * title/menu: unchanged (270 kicks per 30 frames, 0 prims);
  * from frame ~390: 60k–150k VU1 runs per 30 frames, 101k–249k XGKICKs, 73k–181k
    triangle strips, 11k–28k status-flag and 306k–755k clip-flag reads, 0 undecoded ops.
* **Screens (`XSG_SHOT=380,420,480,560`, `XSG_SHOT_VRAM=1`):** still black. VRAM has
  textures (CLUT4 128×128/64×64 at TBP 0x3A00..), and FBP 0x70 is the draw buffer (ZTST
  GEQUAL, zmsk). `XSG_GS_TRACE=450` shows every prim 0x54 (tri-strip, textured) with
  **all vertices at the same screen point (243.2, 335.9)**. That means VU1 gets a matrix
  whose 3×3 part is zero or degenerate (only the screen offset survives), or zero vertex
  data.

## 5. Next

1. Find why the VU1 vertices collapse. Dump `vu1_data.bin` (`XSG_VU_DUMP=450`) and check
   the matrix the renderer uploads (nmlPacketAdd*/xglMatrixStack* →
   `xglMatrixStackSave`). Likely suspects:
   * a camera/world matrix still computed by untranslated or host-divergent C (float-arg
     ABI cases like the ones fixed in parallel);
   * the matrix stack being read before `Vu0CallMatrixStackUnit` ran.
2. Hand-port `m_cam.c:326`.
3. Bit-exact VU rounding (round toward zero) if a PS2 trace comparison shows drift.

## Native VU0 (2026-11-20, #3)

A battle-effect profile (the ST9001 tutorial battle, `native = 1`) had the game thread spend
about a quarter of its time emulating VU0. None of it is emulated now:

* **Microprograms.** The game runs only two VU0 entries, `vu0_callms(32)` (sin, micro 0x0020)
  and `vu0_callms(232)` (cos, 0x00e8). Both are C in `render/vu1_ref/vu0_ref.c`, with the
  interpreter's float rules and everything the program leaves behind (vf1-vf4.x, ACC.x, I,
  vi1/vi2/vi15, MAC, status with the sticky bits). `[debug] vu0_ref = 0` uses the interpreter;
  `[debug] vu0_ref_check = 1` runs both on every call (after a 400,000-input sweep) and logs any
  difference. Any other entry still runs on the interpreter, now on its decoded fast path
  (`RunFast` no longer assumes VU1's memory layout; each unit keeps its own decoded program).
* **Macro op runs.** `scripts/vu0_runs.py` (CMake option `XSG_VU0_RUNS`, on) finds every run of
  two or more consecutive `vu0_arith` / `vu0_move` / `vu0_mr32` calls in `src/` (225 runs, 143
  distinct, 1148 of the 1318 `vu0_arith` call sites) and compiles each distinct run to one SSE
  function in `build/vu0gen/vu0_runs.c`: registers stay in xmm registers, only what the run
  changes is written back, with the final MAC and status. The affected game files are built from
  copies in `build/vu0gen/` with each run replaced by one call (line numbers kept, `#line` to
  the original); `src/` is not touched. `[debug] vu0_runs_check = 1` runs every run both ways and
  logs any difference (165 million runs in a battle: none).
* **Single ops.** A `vu0_arith` outside a run calls a per-op SSE variant (`xsg_vu0_<op>_v/_b`,
  `vu0_inline.h`) chosen at compile time from its constant operands; the game files stay
  without SSE (`-mno-sse`, see Stage 6). `tests/test_vu0_inline.c` checks the variants against
  the generic `vu0_arith`, bit for bit.

The VU1 C references (`render/vu1_ref`) use branchless float helpers, so the compiler vectorises
their per-lane loops (`-O3`), and set 0's strip loop is written with the 4-lane helpers
(`vu_float.h`, `*4`). `[debug] vu1_ref_check = 1` runs every covered VU1 program on both the
reference and the interpreter and compares the GIF output and VU1 data memory (62 million runs
over the opening cutscene and a battle: none differ).
