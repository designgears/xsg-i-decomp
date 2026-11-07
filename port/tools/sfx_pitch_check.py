#!/usr/bin/env python3
"""End-to-end pitch check of the sound output (Stage 10).

Run the game with [debug] wav = mix.wav, spu_trace = dir, sound_log = 1,
log_file = log in settings.ini (XSG_CONFIG).  Every key on then leaves the sample it played in dir
(kNNN_cC_vVV_pPITCH_aADDR.adpcm) and a "[sound] T keyon N ..." line in the log.
For each traced sample this decodes the PS-ADPCM source, finds its dominant
frequency at the SPU2's native 48 kHz, scales it by the voice's pitch register
(0x1000 = 48 kHz) to the frequency the voice should produce, and looks for it
in the recording just after the key on.

usage: sfx_pitch_check.py mix.wav log dir [--max N] [--window S]
"""
import argparse
import glob
import math
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wav_report import read_wav  # noqa: E402

F = [(0, 0), (60, 0), (115, -52), (98, -55), (122, -60)]


def decode(data):
    out, s1, s2 = [], 0, 0
    for b in range(0, len(data) - 15, 16):
        shift, f = data[b] & 15, min((data[b] >> 4) & 7, 4)
        if shift > 12:
            shift = 9
        for i in range(28):
            nib = (data[b + 2 + i // 2] >> ((i & 1) * 4)) & 15
            s = ((nib << 12) - (0x10000 if nib >= 8 else 0)) >> shift
            s += (s1 * F[f][0] + s2 * F[f][1] + 32) >> 6
            s = max(-32768, min(32767, s))
            s2, s1 = s1, s
            out.append(s)
    return np.array(out, dtype=np.float64)


def dominant(x, rate, fmin=40.0):
    if len(x) < 512:
        return None
    x = x[: 1 << int(math.log2(min(len(x), 1 << 16)))]
    spec = np.abs(np.fft.rfft((x - x.mean()) * np.hanning(len(x))))
    freqs = np.fft.rfftfreq(len(x), 1 / rate)
    spec[freqs < fmin] = 0
    i = int(np.argmax(spec))
    if spec[i] <= 0:
        return None
    # parabolic interpolation of the peak
    if 0 < i < len(spec) - 1:
        a, b, c = np.log(spec[i - 1] + 1e-12), np.log(spec[i] + 1e-12), np.log(spec[i + 1] + 1e-12)
        d = 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0
        return (i + d) * rate / len(x)
    return freqs[i]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav")
    ap.add_argument("log")
    ap.add_argument("dir")
    ap.add_argument("--max", type=int, default=40)
    ap.add_argument("--window", type=float, default=0.25)
    a = ap.parse_args()
    rate, mix = read_wav(a.wav)
    mono = mix.mean(axis=1)
    times = {}
    for line in open(a.log, errors="replace"):
        m = re.search(r"\[sound\]\s+([\d.]+) keyon (\d+) core (\d) voice (\d+)", line)
        if m:
            times[int(m.group(2))] = float(m.group(1))
    files = sorted(glob.glob(os.path.join(a.dir, "k*.adpcm")))
    checked = matched = 0
    print(f"{'#':>4} {'t(s)':>7} {'pitch':>6} {'src Hz':>8} {'expect':>8} {'heard':>8}  verdict")
    for path in files[: a.max]:
        m = re.search(r"k(\d+)_c(\d)_v(\d+)_p([0-9a-f]+)_a([0-9a-f]+)", path)
        n, pitch = int(m.group(1)), int(m.group(4), 16)
        t = times.get(n)
        src = decode(open(path, "rb").read())
        f0 = dominant(src, 48000)
        if t is None or f0 is None or pitch == 0:
            continue
        expect = f0 * pitch / 4096
        s = int(t * rate)
        seg = mono[s:s + int(a.window * rate)]
        heard = dominant(seg, rate)
        if heard is None or np.sqrt(np.mean(seg ** 2)) < 1e-4:
            verdict = "silent in the mix"
        else:
            # other voices may dominate: compare within 3 % or an octave of it
            ratio = heard / expect
            ok = any(abs(ratio * k - 1) < 0.03 for k in (1, 2, 0.5))
            verdict = "match" if ok else f"differs (x{ratio:.3f})"
            checked += 1
            matched += ok
        print(f"{n:4d} {t:7.2f} {pitch:#06x} {f0:8.1f} {expect:8.1f} {heard or 0:8.1f}  {verdict}")
    print(f"summary: {matched}/{checked} audible key ons have the expected dominant frequency "
          f"(within 3 %, or an octave of it)")


if __name__ == "__main__":
    main()
