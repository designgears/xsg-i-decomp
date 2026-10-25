#!/usr/bin/env bash
# Keep the mechanical permuter, the trick sweeper and the status page alive
# forever, wide open: every pending body that is not already matched, no score /
# diff / yield gate. Each loop restarts its bot if it exits, so nothing stops.
set -uo pipefail
cd "$(dirname "$0")/../.."
LOG=build/auto_decomp

PERMUTE_ARGS=(--min-score 0 --max-diff 0 --min-p 0 --rounds 20
              --parallel 6 -j 3 --minutes 3 --stall 1 --no-gain 2.5
              --extend 2 --sweep-budget 300)
SWEEP_ARGS=(--watch 10 --jobs 2 --min-score 0 --max-diff 0)

# RUN_PERMUTE=0 keeps the mechanical permuter off (set it to 1 to bring it back).
if [ "${RUN_PERMUTE:-1}" = "1" ]; then
  while true; do
    python3 -B -u scripts/auto_decomp/permute_queue.py "${PERMUTE_ARGS[@]}" \
        >>"$LOG/permute_queue.out" 2>&1
    echo "$(date -u +%FT%TZ) permute_queue exited $? - restarting" >>"$LOG/bots.log"
    sleep 5
  done &
  PERM_PID=$!
else
  echo "$(date -u +%FT%TZ) permuter disabled (RUN_PERMUTE=0)" >>"$LOG/bots.log"
  PERM_PID=
fi

while true; do
  python3 -B -u scripts/auto_decomp/trick_sweep.py "${SWEEP_ARGS[@]}" \
      >>"$LOG/trick_sweep.out" 2>&1
  echo "$(date -u +%FT%TZ) trick_sweep exited $? - restarting" >>"$LOG/bots.log"
  sleep 5
done &
SWEEP_PID=$!

# The read-only status page (what the browser polls every 2 s). Guarded so a
# second copy of this script never fights over the port.
while true; do
  if pgrep -f "status\.py --port ${STATUS_PORT:-8787}" >/dev/null; then
    sleep 30
    continue
  fi
  python3 -B scripts/auto_decomp/status.py --port "${STATUS_PORT:-8787}" \
      >>"$LOG/status.out" 2>&1
  echo "$(date -u +%FT%TZ) status.py exited $? - restarting" >>"$LOG/bots.log"
  sleep 5
done &
STATUS_PID=$!

trap 'kill ${PERM_PID:-} $SWEEP_PID $STATUS_PID 2>/dev/null' INT TERM
echo "permute_queue supervisor $PERM_PID, trick_sweep supervisor $SWEEP_PID, status supervisor $STATUS_PID"
wait