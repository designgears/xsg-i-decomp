# Worker lessons (gcc 2.96 EE, Xenosaga decomp)

What earlier subagents (Muse Spark + Opus) learned when they took functions from 0.6 to 0.99. Read this before you start editing.

## Edits that moved the score (with examples)
- **Address arithmetic and CSE.** Write `p_off` and `b_off` as explicit locals so gcc stops merging `off+base`. In sefAllocScheduler this took the score from 0.61 to 0.83.
- **`addu` operand order.** If a diff shows `addu a,b,c` where the target has `addu a,c,b`, try a `(int)base` cast on one operand. In sefAllocScheduler this fixed both addu operand orders.
- **Splitting modulo/increment expressions.** `nxt = i+1; nxt %= 0x80;` compiles differently from `nxt = (i+1)%0x80`.
- **Globals typed `int` instead of narrower types** can stop gcc hoisting loads early.
- **Single exit with goto.** Write `if (c==0) goto done; ... done: return p-msg-1;` with the return at the end, and drop the redundant `start` var. This fixed the prologue order and the delay-slot layout in MSG_getSize (0.79 to 0.93).
- **Order of updates inside a case.** In MAP_updateUnitSpecialSymbol, swapping `position.y` and `rotation.y` raised the score (0.906 to 0.9125). If the target loads globals in order X then Y, write the C in that order too.
- **Typed struct access instead of a raw global.** `((CardPosWork*)CardPos)->r[i].y` produced the target's `addu v0,v0,s1`. Follow how matched siblings in the same TU access the same data.
- **u64 vs u32 locals, swapping i/j, copying an already-matched sibling's style, per-case store+break instead of a shared goto, reordering error paths, a separate `unsigned char *src` copy pointer.** Each of these closed a near-miss at least once.
- **`volatile`** stopped two read-modify-write ops on one field (`|=1`, `|=0x20`) from merging into one.
- **Statement order around `lw` loads.** Hoisting `packed = src[2];` above `size = src[1];` fixed a `move a3,a0`.

## Plateaus: don't burn runs on these
These rarely respond to source edits. If the only remaining diff is one of these, write down the best score and stop.
- **Pure register-allocation rotations with opcode_score = 1.0.** Examples: lo/hi stuck in t1/t0, a2/v1 swapped in buf setup, s-reg rotations like s2/s3/s4. Workers tried decl order, temps, init order, and int vs u32, and nothing moved them.
- **One `lui/addiu/addu` scheduled before or after a store**, or a few `addiu` base computations in a different order (MAP_createUnitPeer, subMWModeExSet at 0.98).
- **Prologue delay-slot filling**, i.e. which init lands in the `jal` or `bnez` delay slot (CheckSensiCommand 0.93).
- **Branch-likely forms** (`beqzl`/`bnel` vs a plain branch) and `slti` vs `sltiu` coming from `(x-K)&0xFFFF < N`.
- **`jtbl_*` / `D_*` vs `.rodata` relocation names are NOT a blocker.** integrate's data carve gives the compiled C's own jump table / literals the original address automatically. If try_fn still says match:false, look at `differing_words`: those are real code diffs to fix. (The named-double case: `extern double` gives a wrong GPREL `ld`, `extern const double[]` gives `lui v1` not `lui at`; prefer plain literals + carve.)
- **VU0/COP2 code** (`lqc2/vadd/sqc2`). It can't be matched in pure C, and inline asm is banned, so skip it.

## Tooling gotchas
- try_fn can print `match:false` when the only differences are relocation names. integrate.py can still pass in that case (its whole-file check is the real gate), so if every remaining diff is a reloc name, run integrate.
- **gcc ICE in `reload_cse_simplify_operands`** on a 16-byte struct (BLK) copy: try_fn matches but integrate fails in the full TU. Known case: PoBonusCardAnime. Report it and stop.
- If integrate of a sibling in the same TU breaks your compile, fix the body (e.g. a macro name leaking, like `msg` vs `MWMSG`). Don't touch src/.
- If a header moves mid-session and `compile_base` errors with "No such file", stop and report it. Don't create headers.
- If a `.best.json` score doesn't reproduce with `.best.c`, it is stale. Report it.
- Restore the body file to the best-scoring version before you finish.

## Process
- Start from `.best.c`. Make one diff-driven edit per run.
- After roughly 10 runs with no gain on a plateau-type diff, stop and report.
- Your final message needs: the best score, the exact remaining diff, and the variants you tried, so the next worker doesn't repeat them.
