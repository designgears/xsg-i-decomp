#!/usr/bin/env bash
# Serialize integrate/gate critical sections across subagents.
set -euo pipefail
LOCK=${LOCK:-$(cd "$(dirname "$0")/../.." && pwd)/build/auto_decomp/integrate.lock}
mkdir -p "$(dirname "$LOCK")"
exec 9>"$LOCK"
flock 9
exec "$@"
