"""Candidate leases so concurrent batches never work on the same function.

A claim is build/auto_decomp/claims/<unit>__<tu>__<name>.json holding
{key, owner, at, expires}. next.py --claim takes them under claims.lock;
integrate.py releases them. Expired claims (an agent died) are ignored and
reaped.
"""
from __future__ import annotations

import fcntl
import json
import time
from contextlib import contextmanager
from pathlib import Path

from state import STATE_DIR

CLAIMS_DIR = STATE_DIR / "claims"
LOCK = STATE_DIR / "claims.lock"
DEFAULT_TTL_S = 3 * 60 * 60


def _path(k: str) -> Path:
    return CLAIMS_DIR / (k.replace("/", "__") + ".json")


@contextmanager
def locked():
    LOCK.parent.mkdir(parents=True, exist_ok=True)
    with LOCK.open("a") as fh:
        fcntl.flock(fh, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(fh, fcntl.LOCK_UN)


def active(now: float | None = None) -> dict[str, dict]:
    """Live claims by key; reaps expired ones. Caller should hold locked()."""
    now = now or time.time()
    out = {}
    if not CLAIMS_DIR.is_dir():
        return out
    for p in CLAIMS_DIR.glob("*.json"):
        try:
            c = json.loads(p.read_text())
        except (OSError, json.JSONDecodeError):
            p.unlink(missing_ok=True)
            continue
        if c.get("expires", 0) <= now:
            p.unlink(missing_ok=True)
            continue
        out[c["key"]] = c
    return out


def take(k: str, owner: str, ttl_s: int = DEFAULT_TTL_S, model: str | None = None) -> dict:
    """Caller holds locked() and has checked active()."""
    CLAIMS_DIR.mkdir(parents=True, exist_ok=True)
    now = time.time()
    c = {"key": k, "owner": owner, "at": now, "expires": now + ttl_s}
    if model:
        c["model"] = model  # shown by status.py; set by whoever launches the worker
    _path(k).write_text(json.dumps(c) + "\n")
    return c


def release(k: str) -> None:
    with locked():
        _path(k).unlink(missing_ok=True)
