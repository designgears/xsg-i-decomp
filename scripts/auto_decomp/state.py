"""Persistent auto-decomp state under build/auto_decomp/."""
from __future__ import annotations

import fcntl
import json
import time
from contextlib import contextmanager
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STATE_DIR = ROOT / "build/auto_decomp"
STATE_PATH = STATE_DIR / "state.json"
QUEUE_PATH = STATE_DIR / "queue.json"
LOG_PATH = STATE_DIR / "loop.log"
LOCK_PATH = STATE_DIR / "integrate.lock"


@contextmanager
def locked():
    """The global integrate lock: guards src/ edits, unit rebuilds and state.json."""
    LOCK_PATH.parent.mkdir(parents=True, exist_ok=True)
    with LOCK_PATH.open("a") as fh:
        fcntl.flock(fh, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(fh, fcntl.LOCK_UN)


def load_state() -> dict:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    if STATE_PATH.is_file():
        return json.loads(STATE_PATH.read_text())
    return {
        "schema": "auto-decomp/1",
        "matched": [],
        "failed": {},          # name_key -> {attempts, last_error, next_eligible}
        "skipped": {},
        "attempts": 0,
        "matched_count": 0,
        "started_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }


def save_state(state: dict) -> None:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    tmp = STATE_PATH.with_suffix(".tmp")
    tmp.write_text(json.dumps(state, indent=1) + "\n")
    tmp.replace(STATE_PATH)


def key(unit: str, tu: str, name: str) -> str:
    return f"{unit}/{tu}/{name}"


def log(msg: str) -> None:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    line = f"{time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())} {msg}\n"
    with LOG_PATH.open("a") as fh:
        fh.write(line)
