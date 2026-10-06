# Autonomous matching decomp loop

Keeps turning `INCLUDE_ASM` into matching C until the queue is empty, without
breaking `ninja gate`. Built for many agents at once.

## Pieces

| Script | Role |
|---|---|
| `build_queue.py` | Refresh `build/auto_decomp/queue.json` (smallest first) |
| `next.py` | Next N eligible candidates (on claim: rebuilds the prototype and instruction indexes when over an hour old, and starts a detached `../ghidra/refresh.sh` when the Ghidra export is over a day old); `--claim OWNER` leases them, `--per-tu` spreads them; reconciles lost matches; on claim seeds an m2c first draft (`<body>.m2c.c`, and the body if none exists). `--near-miss SCORE`: only functions already at >= SCORE (failure record or `.best.json`), best first, ignoring size/attempt/back-off limits; `--untouched`: only functions nobody has tried |
| `callgraph.py` | Same-TU call graph of the INCLUDE_ASM functions for callee-first scheduling (see below); `--report`, `--key U/T/N`, `--order U/T` |
| `m2c_seed.py` | m2c (mipsee-gcc-c) draft of one function, with the TU's declarations, data files and jump tables as context |
| `fn_index.py` | `build/auto_decomp/fn_index.json`: every in-scope function's instructions from the original images. `next.py` uses it for cross-TU/overlay siblings (>= 0.9 similar, already matched) and holds back an exact copy of a function that is leased, until that one's C can be its sibling (`--report`: duplicate counts) |
| `proto_index.py` | `build/auto_decomp/protos.json`: every TU's function prototypes made self-contained, so m2c drafts type callees from other TUs (rebuilt by `next.py --claim` when over an hour old); also writes `build/ghidra/types/` |
| `../ghidra/export.sh` | Headless Ghidra (EE module): imports the originals with `config/symbols` and the project's own types, writes `build/ghidra/<unit>/<name>.c` with recovered struct layouts; `m2c_seed.py` copies it next to the draft as `<body>.ghidra.c`. Run after `proto_index.py`, ~2 min for all units. `../ghidra/refresh.sh` runs both, then `../ghidra/reseed.py` refreshes every existing `<body>.ghidra.c` from the new export |
| `claims.py` | Leases under `build/auto_decomp/claims/` (60 min TTL, released by integrate) |
| `try_fn.py` | **Lock-free** function-level check: compiles the TU with the body in a private scratch dir, diffs the function against its INCLUDE_ASM bytes (~0.5 s). `"match_after_link": true` (+ `false_lo16` / `lit_literal`: a .lit4/.lit8 data symbol loaded GPREL16 in the target, written as a float literal in ours) = every differing word is a false %lo relocation or such a literal slot (target LO16 -> sym with a reloc-free feeding `lui`, ours the plain immediate `sym & 0xFFFF`; addresses from `config/symbols/<unit>.txt` / `build/<unit>/undefined_syms_auto.txt`): identical after linking, so the pipeline calls `integrate.py --skip-try` |
| `permute_fn.py` | [decomp-permuter](https://github.com/simonlindholm/decomp-permuter) (cloned to `.work/decomp-permuter`) on one near-miss body: direct cc1+as `compile.sh` (checked against `try_fn.py --compile-file`), weights tuned for register/scheduling plateaus, `-j` = cores-2, rebases on its best output after `--stall` minutes without progress, saves every try_fn-confirmed improvement as `.best.c`, stops and integrates as soon as try_fn matches |
| `permute_queue.py` | Runs `permute_fn.py` over near-misses (`state.json` + `.best.json`), one lease (owner `permuter`) each, at most `--rounds` runs per function; skips VU0/sync asm, inline-asm bodies and cc1 ICEs. Cheapest first: (1) **triage** - one `try_fn` on the best body classifies the remaining diff (cached by body mtime in `build/auto_decomp/triage.json`): `match` (incl. try_fn `match_after_link` / `link_check`, integrated with `--skip-try`) / `reloc-only` (bytes equal, only reloc names differ) go to `integrate.py` once, never permuted; `callee` (callgraph `blocked_by` + only branch-likely/delay-slot moves) is skipped for `retry_best.py`; `assembler` (only la/li macro halves moved into/out of delay slots, TU without `-mswap-fix` in `config/tu/as-flags.json`) is skipped and listed as an `-mswap-fix` candidate (as-flags is never edited automatically); the rest is `permute` (a diff that is only a branch-likely bit flip is tagged `branch_likely` and the permuter runs with `--profile branch_likely`: label/goto/condition weights; `trick_sweep.diff_shape` mechanism tags `cmp_fix` / `sched_weight` / `de_ifcvt` / `loop_live` are recorded as `mech` and counted by `--triage-report`). (2) **sweep first** - a best body `trick_sweep` never ran on (`.best.json` `by: sweep`, `sweep_marks.json` sha1, or a loop.log `SWEEP ... done` newer than the body) is swept inline under the same lease; the permuter runs only if that did not match. (3) order: fewest prior permuter rounds, then fewest differing words. `--triage-report [--json]` prints the pool split by category. **Repo rule:** `--min-score 0.85` (ratio) AND `--max-diff 25` (differing instruction words, so a big function at 0.85, i.e. hundreds of wrong words, is left to agents). Defaults: 4 jobs x `-j 3`, 5 min, 1 round |
| `brief.py` | Worker brief for one leased function, context precomputed: current best score + only the changed rows of the diff (no try budget used), `<name>.notes.md` from earlier workers, the closest matched sibling's C, rules, WRAP UP protocol (no time limit stated: the cap is enforced from outside) |
| `upstream_ref.sh` | Read-only snapshot of upstream (`origin/master`) `src/`+`include/` at `build/upstream_ref/`, no merge: upstream recovers typed data we lack; `brief.py` points workers at its copy of their TU |
| `retry_best.py` | No-AI retries of saved `.best.c` bodies (`--after KEY`: same TU + callers of KEY; `--tu`; `--all`): re-scores with `try_function`, integrates matches (`match_after_link` ones with `--skip-try`), updates `.best.json`. Pending = INCLUDE_ASM in src/ (a state.json "matched" key that is INCLUDE_ASM again is retried). integrate.py starts `--after` detached after every MATCH (`AUTO_RETRY=0` disables) |
| `draft_fix.py` | Makes a raw m2c draft compile in its TU (typedefs, `?` types, duplicate/undeclared decls, `unkXX` fields -> offset casts, call arg counts, irregular switch labels), error-driven, no try budget; `m2c_seed.py` runs it on every new seed |
| `clone_twins.py` | No-AI: puts an already-matched twin's C (exact or >= `--min-sim` similar, other TUs/overlays) into a pending function with symbols remapped from the two `.s` files, integrates on match, keeps improvements |
| `trick_sweep.py` | No-AI hill-climb of known mechanical edits (local type flips, adjacent statement swaps, `volatile`, loop forms, casts, ...) on the >= 0.85 / <= 25-diff pool, integrates on match; lease owner `sweep`. Transform families (label prefix: `type`, `stmtswap`, `volatile`, ...) run in **learned order**: per-family attempts / improvements / matches / compiles in `build/auto_decomp/trick_stats.json` (seeded from loop.log SWEEP lines, updated live; `--stats` prints them in try order); best improvement rate first, ties cheaper first, never-helped families last; the first improving batch wins the round. A finished sweep records the body's sha1 in `sweep_marks.json`. Families from worker analyses: `dup_tail_store` (store after an if/else chain into every branch, and back), `regalloc_tie` (register-only diffs: `-dl` .lreg priorities floor_log2(refs)*refs/live, near-tied pseudos mapped to C vars, statement moves across their live ranges), `fallthrough_label` (branch-likely annul bit), `div_hoist` (trap_if scheduling of `/`,`%` array stores), and diff-gated ones (generated only when `diff_shape` of the current try_fn diff names them): `cmp_fix` (branch condition differs and is not an inversion pair, e.g. bgez vs blez: every relational operator / constant k-1,k,k+1 / arm swap on the conditions, zero-compare classes first; matched fileLoad), `sched_weight` (one store moved: re-read / split chained stores / moves / temp / type of the stored variable), `de_ifcvt` (move next to movz/movn: min/max rewritten as branches that pre-reload ifcvt does not take), `loop_live` (address/constant inits reordered before a loop: liveness after the loop). `dup_tail_store`, `regalloc_tie` and `cmp_fix` start with proven priors (`PRIORS`), the other three with exploration priors. `compile_dumps(unit, tu, name, text, dflags)` recompiles the kept .i with any `-d` flags and returns the function's dump sections (`lreg`, `sched`, `sched2`, `ce2`, ...); `sched_ties` uses it for sched1 equal-priority decisions (REG_DEAD) and cmov counts. `--families a,b` runs only those families (no sweep mark), `--all-pending` ignores score / diff / marks (`--all-pending --families cmp_fix`: wrong comparisons are semantic bugs, scan everything). Third-round families (`TEXT_TRANSFORMS`, see the whole body text + ctx `rows`/`unit`/`tu`): `const_fix` (diff-gated: same insn with another immediate -> the C literal equal to our value gets the target's, lui+addiu/ori pairs rebuilt to 32 bits; matched subPrintRibbon), `void_ret` (int without `return <expr>` whose target leaves no value in v0 -> void, or the reverse; conflicting forward declarations in the TU / its src headers are evaluated through `try_function(overrides=...)` and applied with `verified_edit.py` before integrating, recorded as `src_patch` in .best.json; matched _SetVersion3Extent), `lit_inline` (`extern float D_xxx` of a .lit4/.lit8 data symbol -> exact literal; matched checkTalk), `zext_form` (target `andi 0xffff; sltiu`: temp / union / bitfield forms, pre-screened with `snippet.py`; speculative). `text_shape()` gives the text-gated tags (void_ret, lit_inline) to triage. Run them over everything with `--all-pending --families const_fix,void_ret,lit_inline`. Fourth-round families (diff-gated `CTX_TRANSFORMS`, tags also in triage `mech`): `field_alias` (pure reorder moving a non-sp sw/sh/sb while the function loads a same-width global: store the fields through `void *` (cast lvalue or one-field view struct), singles then pairs; cmdCamLock), `volatile_mask` (only an epilogue `ld sN`/`ra` moved among stores: per-store `(volatile T *)` masks over the trailing k<=8 stores, no two consecutive volatile, plus one adjacent swap each), `loop_form` (target loop bottom `move; b(n)ez; andi` vs our `andi; b(n)ez`: goto-entered do-while -> while / for / goto-for(;;) / if-do forms) |
| `snippet.py` | Fast pre-screen: compiles a tiny standalone C snippet with a TU's ee-gcc + cc1 flags (`-S`, no includes, ~0.3 s, cached) and returns the function's insns; `has_seq(insns, [regex...])` checks an asm shape. Used by `zext_form` to drop expression variants before full try_fn compiles. CLI: `snippet.py --unit U --tu T --code 'C text'` |
| `integrate.py` | try_fn precheck → global lock → unit green? → apply → rebuild → keep if identical else restore |
| `pattern_decompile.py` / `run_patterns.py` | Heuristic leaf patterns |
| `verify_unit.py` | Whole-file compare (+ main mapcheck) |
| `state.py` | `state.json`, `loop.log`, and the global lock (`integrate.lock`) |
| `status.py` | Live status page (`http://127.0.0.1:8787/`, read-only): overall/per-unit progress, loop rates, each worker's leases with tries/best score/idle time, permuters, recent events |
| `clear_wip.py` | Reset work in progress (leases, try counters, permuter work, scratch bodies, non-blocked failure records) while keeping every `.best.c` and the `blocked` markers; `--dry-run` first |
| `with_lock.sh` | Run anything (e.g. `ninja gate`) under the same global lock |

Workflow: `.grok/workflows/auto-decomp-batch.rhai` (args: `n`=12, `max_size`=128,
`min_size`, `max_tries`=10, `patterns`=true). Several batches may run concurrently.

## Agent loop

```bash
source scripts/env.sh
python3 -B scripts/auto_decomp/next.py -n 1 --max-size 128 --claim me
# write the body, then iterate (any number of agents in parallel):
python3 -B scripts/auto_decomp/try_fn.py --unit main --tu menu_tec \
  --name MenuTecNextSpeedPointGet --body build/auto_decomp/bodies/main/menu_tec/MenuTecNextSpeedPointGet.c
# once it says "match": true:
python3 -B scripts/auto_decomp/integrate.py --unit main --tu menu_tec \
  --name MenuTecNextSpeedPointGet --body build/auto_decomp/bodies/main/menu_tec/MenuTecNextSpeedPointGet.c
```

## Callee-first scheduling

This gcc 2.96 cc1 only marks a call nothrow when the callee is defined as C
*earlier in the same TU*; otherwise reorg fills delay slots differently
(`beqzl`/`bnel` vs plain branches). A caller whose same-TU callee above it is
still `INCLUDE_ASM` plateaus at a one-word delay-slot diff and matches once the
callee is C. `callgraph.py` reads the `jal` targets of every INCLUDE_ASM
function's `.s` (jalr / other-TU / extern ignored) and records:

- `blocked_by`: same-TU INCLUDE_ASM callees *above* the function (blocking).
  Callees *below* the caller were invisible in the original too and never block.
- `unblocks`: functions transitively waiting on it; `depth` (0 = leaf).

`build_queue.py` copies these into `queue.json` and sorts a function that
unblocks others as if it were up to 4x smaller (`size // (1 + min(unblocks, 3))`).
`next.py` defers blocked candidates to a second pass used only when nothing
unblocked is eligible; `--include-blocked` (or `--key`) lifts that. A blocked
candidate it does return carries `blocked_by` and `blocked_by_note`: decompile
the callee first (or accept a delay-slot-only plateau as expected, not a C bug).

```bash
python3 -B scripts/auto_decomp/callgraph.py --report          # counts, top unblockers, near-misses
python3 -B scripts/auto_decomp/callgraph.py --order ov11/mini_g   # leaf-first order of one TU
```

The report's "near-miss" list (GIVEUP scores in `loop.log` and
`bodies/*/*/*.best.json`, 0.85 <= score < 1, still blocked) are likely free wins
once their `blocked_by` callees are C.

## Invariants

- Only `integrate.py` edits `src/`, always under the global lock and via
  temp-file + rename, so lock-free readers never see a half-written TU.
- Integrate never leaves a red gate: failures restore the exact pre-edit text and
  rebuild. It refuses to run when the unit is already red (no false blame).
- The source is the truth: `next.py` drops `matched` keys whose function is
  `INCLUDE_ASM` again (logged `LOST`) and requeues them.
- Inline asm with control flow or directives (`b*`, `j*`, `jr $31`, `.end`,
  `.section`, ...) is refused: that is the original function pasted back, not C.
  `--allow-asm-body` is the reviewed escape hatch.
- Failures back off 30 min × attempts; 5 attempts retire a candidate.
- New C literals/jumptables trigger `data_carve.py derive` inside integrate.
- Manual builds while agents run: `scripts/auto_decomp/with_lock.sh ninja gate`.
- `config/tu-build.json` / `config/tu/data-carves.json` stay gitignored; regenerate with `scripts/synthesize_public_manifests.py` after a full configure.
