"""Run ee-gcc 2.96's cc1 with address-space randomisation off.

ee-gcc 2.96 can hash heap addresses (CSE hashes SYMBOL_REF string pointers),
so the host memory layout is in principle an input to code generation
(Megami-Decomps/dds-decomp, docs/toolchain.md). `setarch -R` (personality
ADDR_NO_RANDOMIZE) is inherited by the driver's children, so wrapping the
ee-gcc driver call covers cc1. The generated build files (tools/tu/ninja_main.py,
tools/tu/cc_tu.sh) spell the same prefix literally; Python call sites use
NOASLR.

A second, measured input is the length of the file names cc1 sees (they shift
its heap): scratch compiles use names as long as the real build's
(scripts/auto_decomp/try_fn.py same_len_link).
"""
import shutil

NOASLR_SH = "setarch -R"
NOASLR = ["setarch", "-R"] if shutil.which("setarch") else []
