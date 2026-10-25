# Public header inputs

`tools/header_harvest.py` regenerates `include/` with Python 3.10 or newer and
the standard library. Run it from the repository root, then run it again with
`--check`. Header generation needs neither a game binary nor a private queue.
Exact matching still requires the ordinary build and whole-file gate.

The files here supply the historical declarations, canonical decisions, TU
ownership and exact-hash source witnesses required by that generator. They are
reading inputs, not acceptance records or recovery credit. The generator uses
the live file under `config/` when present, otherwise its matching path under
`tools/header_inputs/config/`. A public clone therefore uses this directory.
Its TU manifest omits build configuration except for identities pinned by the
owner checks, and its unit records contain only declaration-reading inputs.
Machine-specific paths become repository-relative, and derived metadata hashes
are recalculated for that public representation. Source-witness hashes and the
private acceptance records remain unchanged.

Edit a type in its defining TU's `src/` header, rather than in a generated
`include/` header. Shared historical declarations and canonical decisions with
no live TU definition are in the JSON inputs here. Do not change hash-pinned
historical declarations or witnesses to bypass an ownership check.

For example, propose a same-size field extension of `CalcUnitParam`:

```bash
python3 -B tools/header_types.py propose \
  --type CalcUnitParam --file src/ov01/calc.h \
  --replace 'unsigned char unmodeled_17[0x1C - 0x17];' \
  --with 'unsigned char unmodeled_17;' \
  --with 'u16 conditions;' \
  --with 'unsigned char unmodeled_1a[0x1C - 0x1A];' \
  --evidence 'ov01 specAnalyzeProc: unsigned halfword at +0x18' \
  --by YOUR_NAME -o /tmp/calc-conditions.json
python3 -B tools/header_types.py check /tmp/calc-conditions.json
python3 -B tools/header_types.py apply /tmp/calc-conditions.json
python3 -B tools/header_harvest.py
python3 -B tools/header_harvest.py --check
```

Include the actual original-instruction evidence in a submitted proposal.
`check` verifies that the extension preserves the known members and byte size;
it does not establish the proposed semantics or byte-identical compilation.

Maintainers refresh the distributable inputs from the live metadata with:

```bash
python3 -B tools/header_harvest.py export-inputs --out tools/header_inputs
```

This preserves the private records and copies only the inputs needed for
header generation. Source witnesses keep their original SHA-256. Verify a
public export after refreshing; its generated headers must agree with those
from the live metadata.
