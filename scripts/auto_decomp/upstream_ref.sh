#!/usr/bin/env bash
# Read-only snapshot of upstream's src/ and include/ (origin/master, no merge) at
# build/upstream_ref/. Upstream recovers typed data (struct layouts, tables,
# statics) we have not; brief.py points workers at its copy of their TU.
#   scripts/auto_decomp/upstream_ref.sh [REF]     (default origin/master; fetches first)
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd); REF=${1:-origin/master}
cd "$ROOT"; git fetch -q origin
out=build/upstream_ref; rm -rf "$out.new"; mkdir -p "$out.new"
git archive "$REF" src include | tar -x -C "$out.new"
git rev-parse --short "$REF" > "$out.new/REV"
rm -rf "$out"; mv "$out.new" "$out"
echo "upstream_ref: $(cat "$out/REV")"
