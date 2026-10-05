#!/usr/bin/env bash
# Serialize integrate/gate critical sections across subagents.
set -euo pipefail
LOCK=${LOCK:-/home/bobby/Projects/decomp/xsg-i-decomp/build/auto_decomp/integrate.lock}
mkdir -p "$(dirname "$LOCK")"
exec 9>"$LOCK"
flock 9
exec "$@"
