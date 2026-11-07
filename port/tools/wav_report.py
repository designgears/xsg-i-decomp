#!/usr/bin/env python3
"""Report on a recorded mix (XSG_WAV=path, Stage 10): per-second RMS and peak
level, clipping, and the dominant frequencies, so sound output can be checked
without listening.

usage: wav_report.py file.wav [--from S] [--to S] [--step S] [--peaks N] [--gap-ms MS]

Columns: time, RMS L/R in dBFS (-inf = digital silence), peak dBFS, the
fraction of samples at full scale, and the N strongest spectral peaks of the
mono mix (Hz, with their level relative to the strongest).  The summary also
counts dropouts: runs of at least --gap-ms (default 20) ms where both channels
stay within 2 LSB of zero, between sounding windows (a starved stream plays
cleared, silent blocks).  Needs numpy.
"""
import argparse
import math
import struct
import sys

import numpy as np


def read_wav(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise SystemExit("not a RIFF/WAVE file")
    pos, fmt, pcm = 12, None, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack_from("<I", data, pos + 4)[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", body)
        elif cid == b"data":
            pcm = body if size else data[pos + 8:]  # header may lag a live recording
            if len(pcm) < size:
                pcm = data[pos + 8:]
            break
        pos += 8 + size + (size & 1)
    if fmt is None or pcm is None:
        raise SystemExit("no fmt/data chunk")
    tag, ch, rate, _, _, bits = fmt
    if tag != 1 or bits != 16:
        raise SystemExit("only 16-bit PCM is supported")
    n = len(pcm) // (2 * ch)
    x = np.frombuffer(pcm[:n * 2 * ch], dtype="<i2").reshape(n, ch).astype(np.float64) / 32768.0
    return rate, x


def db(v):
    return -math.inf if v <= 0 else 20 * math.log10(v)


def peaks(mono, rate, count):
    if len(mono) < 1024 or not np.any(mono):
        return []
    w = np.hanning(len(mono))
    spec = np.abs(np.fft.rfft(mono * w))
    freqs = np.fft.rfftfreq(len(mono), 1 / rate)
    spec[freqs < 20] = 0
    found = []
    order = np.argsort(spec)[::-1]
    top = spec[order[0]] if len(order) else 0
    for i in order:
        if spec[i] <= 0 or len(found) >= count:
            break
        f = freqs[i]
        if all(abs(f - g) > max(15.0, 0.03 * g) for g, _ in found):
            found.append((f, db(spec[i] / top) if top else -math.inf))
    return found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav")
    ap.add_argument("--from", dest="t0", type=float, default=0)
    ap.add_argument("--to", dest="t1", type=float, default=None)
    ap.add_argument("--step", type=float, default=1.0)
    ap.add_argument("--peaks", type=int, default=3)
    ap.add_argument("--gap-ms", type=float, default=20)
    a = ap.parse_args()
    rate, x = read_wav(a.wav)
    dur = len(x) / rate
    print(f"{a.wav}: {rate} Hz, {x.shape[1]} ch, {dur:.2f} s")
    t1 = dur if a.t1 is None else min(a.t1, dur)
    step = int(a.step * rate)
    loud, silent, clipped = 0, 0, 0
    print(f"{'t(s)':>7} {'rmsL':>7} {'rmsR':>7} {'peak':>7} {'clip%':>6}  dominant Hz (dB rel.)")
    t = a.t0
    while t < t1 - 1e-9:
        s, e = int(t * rate), min(int(t * rate) + step, len(x))
        seg = x[s:e]
        rl, rr = np.sqrt(np.mean(seg ** 2, axis=0))[:2] if x.shape[1] > 1 else (np.sqrt(np.mean(seg ** 2)),) * 2
        pk = np.max(np.abs(seg))
        clip = np.mean(np.abs(seg) >= 32767 / 32768.0) * 100
        p = peaks(seg.mean(axis=1), rate, a.peaks)
        ps = "  ".join(f"{f:7.1f} ({d:5.1f})" for f, d in p)
        print(f"{t:7.1f} {db(rl):7.1f} {db(rr):7.1f} {db(pk):7.1f} {clip:6.2f}  {ps}")
        if pk == 0:
            silent += 1
        elif max(rl, rr) > 10 ** (-60 / 20):
            loud += 1
        if clip > 0.1:
            clipped += 1
        t += a.step
    total = math.ceil((t1 - a.t0) / a.step)
    seg = x[int(a.t0 * rate):int(t1 * rate)]
    quiet = np.all(np.abs(seg) <= 2 / 32768.0, axis=1).astype(np.int8)
    edges = np.diff(np.concatenate(([0], quiet, [0])))
    starts, ends = np.where(edges == 1)[0], np.where(edges == -1)[0]
    runs = [(st, en) for st, en in zip(starts, ends)
            if en - st >= a.gap_ms * rate / 1000 and st > 0 and en < len(seg)]
    if runs:
        where = ", ".join(f"{a.t0 + st / rate:.2f}s+{(en - st) * 1000 / rate:.0f}ms" for st, en in runs[:8])
        print(f"dropouts (>= {a.gap_ms:.0f} ms of silence inside the range): {len(runs)}: {where}"
              + (" ..." if len(runs) > 8 else ""))
    else:
        print(f"dropouts (>= {a.gap_ms:.0f} ms of silence inside the range): 0")
    print(f"summary: {total} windows, {loud} above -60 dBFS RMS, {silent} digitally silent, "
          f"{clipped} with >0.1% clipped samples; overall RMS "
          f"{db(float(np.sqrt(np.mean(seg ** 2)))):.1f} dBFS, peak {db(float(np.max(np.abs(seg)))):.1f} dBFS")


if __name__ == "__main__":
    sys.exit(main())
