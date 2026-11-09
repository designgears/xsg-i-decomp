#!/bin/bash
# Full ByteMatch pipeline in the current checkout: gate + verification + native objdiff report.
# Usage: cd <checkout> && tools/dataown/report.sh [UNIT/STEM ...]   (prints overall measures, then each named TU's sections)
cd "$(git rev-parse --show-toplevel)" && source scripts/env.sh >/dev/null
.venv/bin/python -B tools/tu/verify_report.py --begin >/dev/null || exit 1
.venv/bin/python -B configure.py --quiet >/dev/null 2>&1
rm -f build/gate.json; ninja gate >/tmp/gate.$$.log 2>&1 || { echo "GATE FAILED"; grep -E "error|FAILED|differ|backwards" /tmp/gate.$$.log | head; exit 1; }
.venv/bin/python -B tools/tu/verify_report.py --finish 2>&1 | tail -1 | grep -v "verification.json$"
OBJDIFF_CLI=$PWD/.work/toolchains/objdiff-cli .venv/bin/python -B tools/objdiff_report.py -o build/report.json 2>&1 | grep -v "INFO\|Native objdiff"
python3 - "$@" <<'P'
import json,sys
r=json.load(open('build/report.json'));m=r['measures']
print({k:m.get(k) for k in ['matched_code_percent','matched_functions','matched_data_percent','complete_code_percent']})
for u in r['units']:
    if u['name'] in sys.argv[1:]:
        print(u['name'],[(s['name'],s.get('size'),s.get('fuzzy_match_percent')) for s in u['sections'] if s['name']!='.text'])
P
