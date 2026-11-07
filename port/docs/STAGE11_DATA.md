# Stage 11: data relocation and symbol-overlap audit

Stage 11 checks two kinds of data bug that each turned up as single cases before:

1. **Integers that splat symbolised.** A data word whose value falls in the RAM
   range becomes `.word D_xxx` / `.word Sym + off`. gen_host_data.py
   (STAGE3_BOOT.md) then turns it into a host relocation. Earlier cases were
   asVramBase (STAGE8_GFX.md), Head_9/TestPrim CLAMP_1 (STAGE10_COMPARE.md §3.3)
   and the string tails (STAGE7_EVENTS.md).
2. **One object, two C names.** A file names both A and B, where B lies inside A on
   the PS2. GCC assumes the two never overlap. This is the xglFontFlushCore hang
   (STAGE10_PERF.md §1a, finding 5a).

## 1. Symbolised integers

### Audit: `port/scripts/audit_data_relocs.py`

```
python3 port/scripts/audit_data_relocs.py --host port/build/xsg [--min 3] [--all]
#  score unit vram label+off value ref status reasons
```

The audit walks the same objects as gen_host_data.py (it imports its linker-script
and ninja parsing). Each symbolised word gets:

* **The original value.** These are the SLUS_204.69 / OV*.OVL bytes that splat
  copies into the line comment. A real pointer's value equals the PS2 address of
  its target.
* **What the host does with it.** One of `reloc`, `raw` (NO_RELOC_LABELS or
  `string_tail()`) or `abs` (the symbol is an absolute `--defsym` such as
  `D_8000 = 0x8000`, so the host value equals the PS2 value and nothing breaks).
  Only `reloc` words are ranked.
* **A score.** It adds evidence against a pointer and subtracts evidence for one:

| evidence | score |
|---|---|
| GIF A+D data: the qword's word 2 is a GS register id (CLAMP, TEX0/1, FRAME, ZBUF, SCISSOR, XYOFFSET, …) and word 3 is 0. Also the high word of a GIFtag | +6 |
| value is the start of a real object (sized symbol_addrs entry, splat data label, or a symbol that C/function asm uses) | −4 |
| not such a start | +2 |
| … and only reachable as `sym+off` (+1 aligned, +2 unaligned), offset into code | +2 |
| … but an aligned offset inside the object `sym` starts (struct member, malloc bins) | −3 |
| u16-pair shape (low half 0, or both halves < 0x400) | +1 |
| unaligned value | +2 |
| neighbours (±4 words) are ints/floats/misses | +2 |
| neighbours are real pointers | −3 |
| C declares the label as an integer array | +2 |

Before the fix, the audit counted 10044 relocated words, 176 raw and 198 absolute. It
ranked **367 words in 80 labels** as suspicious (score ≥ 3). Grouped by label, all 80
have no symbolised word that hits a real object start. There was one exception, which
is a false positive: `D_004C7738` (menu_shop tabs) holds `{ShopData + k*0x28, n, m}`.
Those are real pointers into the unsized ShopData array, and a TEX1 register id
happens to sit in one of its qwords. Six more labels had only "aligned interior"
words (CLAMP values such as `_imageMapper + 0x4B0B0`, a BMP header) and were confirmed
from the C type.

### Confirmed and fixed: 85 labels, 412 words

Each label was checked against its C declaration and its bytes. All 85 are now in
`NO_RELOC_LABELS` (gen_host_data.py), each with a one-line reason. That makes 88
entries with the three earlier ones. After the fix the audit counts 9632 relocated words
and 588 raw. Only the D_004C7738 false positive and one ov01 pointer table (keyCmdTblCha)
still score ≥ 3.

| group | labels | what the word really is |
|---|---|---|
| GIF packets | TestPrim_0_00362790 (map_1), Head_13 (game_defocus), TestEnv_0_00369BA0 (game_radar), TestEnv_1_0036DDB0 (karakama_test), TestEnv_49/56/59/60 (endou_test), sPoint_0 (init_uwamono_sys) | CLAMP_1/2 values 0x007FC005, 0x003FC000, 0x003FDFF0, 0x003FFFF0, 0x007FDFF0 (MINU/MAXU/MINV fields); RGBAQ 0x005151EE |
| u/v tables | 17 `*_uv` tables (end_print: others, ether, icom, pad, pad2, face00–04, face_agws, seg, btl_msg, btl_msg2, umn, thumnail) | u16 u,v,w,h rectangles, `unsigned char[]` in C |
| struct/int tables | _srsChrEfTbl2 (`short chrNo,min,max`), TrapKind_t_0 (`flags`), line_34/d_42 (`EndouPrintPoint` RGBA), D_004C92E8 (u16 ranges), BmpHeader, headerALL (JPEG Huffman tables), list (hair_test name tails), two_over_pi_004D6220 (libm `__ieee754_rem_pio2` digits) | integers |
| copied MIPS | osdsrc, srcfile (InitExecPS2), tlbsrc, kernelTLB, defaultTLB, extendTLB (set_tlb_handler), eenull (glue_2) | instructions / TLB entries copied into the EE kernel; the host never runs them |
| strings | 43 labels (`D_004C21C8` "\x0b\x0c\x80\0\0OFF", `D_004DA9D8` "\x01Hp", `D_004DB188` "\x01Target", "\x0b%s", "%2d" debug formats, SJIS text such as text_17, D_004C2BE0) | NUL-terminated tails that `string_tail()` misses because they contain control codes (0x01, 0x0b, 0x0c, 0x0d, 0x19) or SJIS/EUC bytes |

None of these was a crash. Effects of the old relocation:

* GIF packets: wrong REGION_CLAMP rectangles for the map, depth-of-field (defocus)
  and radar textures. This is the same failure as the Head_9 shadow bug. No visible
  difference was found on the test route (see Verification). Probably the wrap mode
  in use ignores MIN/MAX, but this was not checked.
* uv tables: wrong sprite rectangles in the end_print battle UI (icons, pad
  glyphs, face sprites).
* Strings: a host pointer in place of the last 2–3 characters plus NUL, so "Hp",
  "Weight", "Target", "Result", "Wt" lost their tails and could run on into
  neighbouring bytes. Fonts printed the address bytes.
* `two_over_pi`: wrong argument reduction for sin/cos with |x| ≥ 2^19·π/2 (never in
  practice).

### Verification

The run is `XSG_GS=gpu XSG_GPU_HEADLESS=1 XSG_AUDIO=0 XSG_MPEG=0 XSG_SKIP_EVENTS=1 XSG_FAST=1
XSG_PAD='300:START,1100:START,1200:CROSS,1500:CIRCLE*400@60'`, with shots at frames
140/1000/1400/1700/6000. It was compared with `compare -metric AE` against the
build before Stage 11. **All five frames have 0 differing pixels.** This holds both
after the data fix and after the Stage 11 source change. The recipe ends in the
field with the radar on screen (frame 6000), and the radar pixels match. The fixed
labels drive screens this route does not reach (battle UI, shop/ether/agws menus,
debug overlays, map_1 test packet) or values the GS ignores in the active wrap
mode. No regression either.

## 2. One object under two C names

### Checker: `port/scripts/find_overlap_access.py`

```
python3 port/scripts/find_overlap_access.py port/build [-v]
```

The checker reads the **compiled host objects**. A source-level check would have to
guess what `-O2` folds. Per game object:

* Every `R_386_32` relocation in code against a data symbol gives the constant
  offset GCC emitted (the REL addend in place). The instruction that holds it
  classifies the use: `mem` (memory operand), `cmp` (cmp/sub/lea with an
  immediate, usually an end pointer), `addr` (address taken), or `hi`
  (`mov $A+0x8000,%eax; xor %ax,%ax`, the translated MIPS `%hi(A)`).
* DWARF (`llvm-dwarfdump`) gives the declared size of every variable.
* The PS2 layout comes from `build/<unit>/symbol_addrs.txt` (plus main's for
  overlays) and the splat data labels.

For every pair (A, B) that one file names, with B = A + gap on the PS2:

* `OVERLAP`: a `mem` use of A at offset ≥ gap. The build fails.
* `EXTENT`: no constant offset reaches B, but `sizeof(A)` as declared > gap, so a
  variable index can reach it. The build fails.
* `review` (`-v` only): only `addr` uses reach B.
* `cmp`/`hi` uses are ignored.

`find_overlap_access_allow.txt` lists the pairs reviewed as harmless, with a reason
each. CMake runs the checker in the link step (`xsg` custom command) before
link_xsg.py. It cannot run at configure time because it needs the objects. A
new unreviewed pair stops the build in the same way find_alias_views.py stops the
configure.

### Results

Of the 258 objects that use two symbols < 0x40 apart, the large majority never reach
past their own symbol.

| result | count | outcome |
|---|---|---|
| OVERLAP | 1 | **fixed**: menu_agws_para_set.c `AgwsInfoMain` reads `msg01_2_0036D990[5]` and `[weaponHand + 2]`, past its four entries into `msg02_3_0036D9A0`. On the host this is now `#define msg02_3_0036D9A0 (msg01_2_0036D990 + 4)` (PORT_HOST only), the FS treatment. Both tables are read-only, so there was no visible effect. |
| EXTENT | 30 | allow-listed. All are read-only: format strings and name tables declared larger than the symbol (`char D_004DBA28[0x10]`, `const char D_004DB6E8[32]`, …), `Head_8` (u64[12] packet over `rate_9`), `GameBgDrawType2Param` over `GameIdLight`, and `batDatBuf[0x10000]` over its own fields D_426F48..D_426F8C. batDatBuf is filled by `dataFileLoad()`, a call after which GCC reloads every global, and only read after that. |
| review | 29 | not failures. Each one checked was a loop bound or end pointer that GCC materialised as `A + n·stride` (`actor + 64·0xA94 + 0x8dc`, `plChaData + 0x20·0xA8 + 0x4e`, `UmnWork + 0x80`, `SyncDCache(&ClearEnv, &ClearEnv + 1)`). |

A hazard needs a store through one name and a load through the other in the same
function, with no call in between. None of the EXTENT or review pairs has that.
When a pair is added to the allow list, the reason must say why.

## Files

* `port/scripts/audit_data_relocs.py`: ranking of symbolised data words (report only).
* `port/scripts/gen_host_data.py`: `NO_RELOC_LABELS` is now a dict `label -> reason`.
* `port/scripts/find_overlap_access.py` and `find_overlap_access_allow.txt`: build-time check.
* `src/main/menu_agws_para_set.c`: PORT_HOST `#define` for `msg02_3_0036D9A0`.
