#!/usr/bin/env python3
"""Xenosaga PC port status site: live screen, screenshots, progress, metrics.

Stdlib only.  Runs a private copy of port/build/xsg in a loop, configured
only by a per-run settings file (LIVE_DIR/settings.ini, passed as XSG_CONFIG;
the port reads no other environment variable, port/docs/MENU.md), with
[debug] shot_every so the page can show the latest frame, and serves:
  /            the page (mobile-first; dense dashboard on wide screens)
  /live.png    newest live frame
  /shot/<name> a gallery image
  /doc/<name>  a port/docs/*.md file (plain text)
  /api/status  JSON (runner, log, commits, gallery, metrics)
  /ws/pad      WebSocket: the web controller (port/docs/STATUS_SITE.md)
  POST /api/pad      the same pad update as one JSON body (fallback)
  POST /api/restart  restart the game (interactive mode)

Usage: python3 port/tools/status_site/server.py [--port 8765] [--no-runner]
         [--pad 300:0x0800 | --interactive] [--live-dir /tmp/xsg-live] [--bin port/build/xsg]
With no --pad the run is interactive: no pad script, no timeout, the game is
restarted only when it exits (or from the page), and the page's controller
drives it through [debug] remote_pad = LIVE_DIR/pad.bin.
"""
import argparse, base64, collections, hashlib, json, os, re, shutil, socket, struct, subprocess, threading, time
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BUILD_BIN = ROOT / "port/build/xsg"
LIVE_DIR = Path("/tmp/xsg-live")
INTERACTIVE = False
GALLERY_DIRS = [ROOT / "port/docs/screenshots", ROOT / "port/build/shots"]
DOCS = ROOT / "port/docs"
BASE_BRANCH = "decomp-work"

state = {"running": False, "started": None, "runs": 0, "exit": None, "log": [], "bin_mtime": None,
         "pid": None, "lines": 0, "tags": collections.Counter(), "frame": None, "frame_t": None,
         "fps": None, "history": []}
lock = threading.Lock()
FRAME_RE = re.compile(r"\bframe\s+(\d+)")
TAG_RE = re.compile(r"^\[([a-zA-Z0-9_]+)\]")


def write_settings(path, shot_every, run_seconds, pad, frames):
    """The live run's settings.ini: headless xsg-gpu, no audio, no movies,
    skipped events, the pad script, periodic shots, the stream FIFO."""
    stream = str(STREAM_FIFO) if STREAM_FIFO.exists() else ""
    path.write_text(f"""# Written by port/tools/status_site/server.py for each live run.
[renderer]
gs = gpu

[game]
movies = 0
memcard_dir = {LIVE_DIR / "mc"}

[audio]
enabled = 0

[debug]
headless = 1
gpu_bin = {BUILD_BIN.parent / "gpu/xsg-gpu"}
skip_events = 1
pad = {pad}
pad_log = 1
remote_pad = {PAD_FILE}
shot_every = {shot_every}
shot_dir = {frames}
stream = {stream}
timeout = {"" if INTERACTIVE else run_seconds}
""")


game = {"proc": None, "restart": False}


def runner(shot_every, run_seconds, pad):
    LIVE_DIR.mkdir(parents=True, exist_ok=True)
    while True:
        game["restart"] = False
        if not BUILD_BIN.exists():
            time.sleep(5); continue
        exe = LIVE_DIR / "xsg"  # private copy so rebuilds don't disturb the running instance
        shutil.copy2(BUILD_BIN, exe)
        frames = LIVE_DIR / "frames"
        frames.mkdir(exist_ok=True)
        for f in frames.glob("*.png"):
            f.unlink(missing_ok=True)
        cfg = LIVE_DIR / "settings.ini"
        write_settings(cfg, shot_every, run_seconds, pad, frames)
        # only XSG_CONFIG: stale XSG_* in the server's own environment would just be logged as ignored
        env = {k: v for k, v in os.environ.items() if not k.startswith("XSG_")}
        env["XSG_CONFIG"] = str(cfg)
        p = subprocess.Popen([str(exe)], cwd=str(ROOT), env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, errors="replace")
        game["proc"] = p
        with lock:
            state.update(running=True, started=time.time(), exit=None, log=[], pid=p.pid, lines=0,
                         tags=collections.Counter(), frame=None, frame_t=None, fps=None,
                         bin_mtime=BUILD_BIN.stat().st_mtime)
            state["runs"] += 1
        for line in p.stdout:
            line = line.rstrip("\n")
            with lock:
                state["log"].append(line)
                del state["log"][:-400]
                state["lines"] += 1
                m = TAG_RE.match(line)
                if m:
                    state["tags"][m.group(1)] += 1
                m = FRAME_RE.search(line)
                if m:
                    n, t = int(m.group(1)), time.time()
                    if state["frame"] is not None and t > state["frame_t"] + 0.5 and n >= state["frame"]:
                        state["fps"] = (n - state["frame"]) / (t - state["frame_t"])
                        state["frame"], state["frame_t"] = n, t
                    elif state["frame"] is None:
                        state["frame"], state["frame_t"] = n, t
        rc = p.wait()
        game["proc"] = None
        with lock:
            state.update(running=False, exit=rc, pid=None)
            state["history"].append({"run": state["runs"], "exit": rc, "secs": round(time.time() - state["started"]),
                                     "lines": state["lines"], "frame": state["frame"]})
            del state["history"][:-20]
        time.sleep(0.5 if game["restart"] else 3)


def restart_game():
    p = game["proc"]
    if not p:
        return False
    game["restart"] = True
    p.terminate()
    try:
        p.wait(5)
    except subprocess.TimeoutExpired:
        p.kill()
    return True


# ---- web controller ------------------------------------------------------
# SCE_PAD masks (the names of the [debug] pad script, port/runtime/sdk.c)
PAD_BITS = {"SELECT": 0x0100, "L3": 0x0200, "R3": 0x0400, "START": 0x0800, "UP": 0x1000, "RIGHT": 0x2000,
            "DOWN": 0x4000, "LEFT": 0x8000, "L2": 0x0001, "R2": 0x0002, "L1": 0x0004, "R1": 0x0008,
            "TRIANGLE": 0x0010, "CIRCLE": 0x0020, "CROSS": 0x0040, "SQUARE": 0x0080}
PAD_FILE = LIVE_DIR / "pad.bin"
CLIENT_TIMEOUT = 0.5   # a client silent this long is released
REFRESH = 0.2          # the file is rewritten at least this often (the game drops records > 1 s old)
MIN_WRITE = 0.004      # at most ~250 file writes/s, however many messages arrive
MAX_CLIENTS = 16
pad = {"clients": {}, "seq": 0, "mask": 0, "sticks": [128] * 4, "cond": threading.Condition(), "dirty": True,
       "msgs": 0, "writes": 0, "last_write": 0.0, "next_id": 0}


def pad_parse(d):
    """{buttons: [names] | mask, lx, ly, rx, ry} -> (mask, [lx, ly, rx, ry])."""
    b = d.get("buttons", 0)
    if isinstance(b, (list, tuple)):
        m = 0
        for n in b[:32]:
            m |= PAD_BITS.get(str(n).upper(), 0)
    else:
        m = int(b) & 0xffff
    st = []
    for k in ("lx", "ly", "rx", "ry"):
        v = d.get(k, 128)
        st.append(max(0, min(255, int(v if isinstance(v, (int, float)) else 128))))
    return m, st


def pad_update(cid, d):
    m, st = pad_parse(d)
    now = time.monotonic()
    with pad["cond"]:
        c = pad["clients"].setdefault(cid, {"mask": 0, "st": [128] * 4, "st_t": 0.0})
        c["t"] = now
        pad["msgs"] += 1
        if st != c["st"]:
            c["st_t"] = now
        c["mask"], c["st"] = m, st
        pad["dirty"] = True
        pad["cond"].notify()


def pad_drop(cid):
    with pad["cond"]:
        if pad["clients"].pop(cid, None) is not None:
            pad["dirty"] = True
            pad["cond"].notify()


def pad_merge(now):
    """OR of the live clients' buttons; each stick from the client that moved it last (if off centre)."""
    m, st = 0, [128] * 4
    for cid in [k for k, c in pad["clients"].items() if now - c["t"] > CLIENT_TIMEOUT]:
        del pad["clients"][cid]
    best = [-1.0, -1.0]
    for c in pad["clients"].values():
        m |= c["mask"]
        for k in (0, 2):
            off = abs(c["st"][k] - 128) > 24 or abs(c["st"][k + 1] - 128) > 24
            if off and c["st_t"] > best[k // 2]:
                best[k // 2] = c["st_t"]
                st[k], st[k + 1] = c["st"][k], c["st"][k + 1]
    return m, st


def pad_write(m, st):
    pad["seq"] = (pad["seq"] + 1) & 0xffffffff
    ms = int(time.time() * 1000) & 0xffffffff
    # pressures 0: the game uses full pressure for each held button
    rec = b"XSGP" + struct.pack("<II4B12xI", pad["seq"], m, st[0], st[1], st[2], st[3], ms)
    tmp = PAD_FILE.with_name(PAD_FILE.name + ".tmp")
    with open(tmp, "wb") as f:
        f.write(rec)
    os.replace(tmp, PAD_FILE)
    pad["writes"] += 1


def pad_writer():
    """Writes PAD_FILE on every change (throttled) and every REFRESH; expires silent clients."""
    PAD_FILE.parent.mkdir(parents=True, exist_ok=True)
    while True:
        with pad["cond"]:
            pad["cond"].wait_for(lambda: pad["dirty"], timeout=0.05)
            now = time.monotonic()
            gap = now - pad["last_write"]
            if gap < MIN_WRITE:
                wait = MIN_WRITE - gap
            else:
                wait = 0
                n0 = len(pad["clients"])
                m, st = pad_merge(now)
                if pad["dirty"] or gap >= REFRESH or len(pad["clients"]) != n0 or m != pad["mask"] or st != pad["sticks"]:
                    pad["dirty"] = False
                    pad["mask"], pad["sticks"] = m, st
                    pad["last_write"] = now
                    try:
                        pad_write(m, st)
                    except OSError:
                        pass
                    pad["cond"].notify_all()
        if wait:
            time.sleep(wait)


def pad_state():
    with pad["cond"]:
        return {"mask": pad["mask"], "sticks": pad["sticks"], "clients": len(pad["clients"]), "seq": pad["seq"],
                "buttons": [n for n, b in PAD_BITS.items() if pad["mask"] & b]}


WS_GUID = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def ws_send(sock, payload, op=1):
    if isinstance(payload, str):
        payload = payload.encode()
    n = len(payload)
    hdr = bytes([0x80 | op]) + (bytes([n]) if n < 126 else bytes([126]) + struct.pack(">H", n))
    sock.sendall(hdr + payload)


def ws_recv(rf):
    """One frame -> (opcode, payload); None on EOF.  Client frames are masked."""
    h = rf.read(2)
    if len(h) < 2:
        return None
    op, n = h[0] & 15, h[1] & 127
    if n == 126:
        n = struct.unpack(">H", rf.read(2))[0]
    elif n == 127:
        n = struct.unpack(">Q", rf.read(8))[0]
    if n > 4096:
        raise ValueError("frame too large")
    key = rf.read(4) if h[1] & 0x80 else b"\0\0\0\0"
    data = rf.read(n)
    if len(data) < n:
        return None
    return op, bytes(b ^ key[i & 3] for i, b in enumerate(data))


STREAM_FIFO = LIVE_DIR / "stream.fifo"
video = {"jpeg": None, "seq": 0, "fps": None, "cond": threading.Condition(), "ok": False}


def video_loop(quality):
    """Raw RGB frames from the game's XSG_STREAM FIFO -> ffmpeg -> MJPEG frames."""
    if not shutil.which("ffmpeg"):
        return
    LIVE_DIR.mkdir(parents=True, exist_ok=True)
    if not STREAM_FIFO.exists():
        os.mkfifo(STREAM_FIFO)
    video["ok"] = True
    while True:
        p = subprocess.Popen(["ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "rawvideo",
                              "-pix_fmt", "rgb24", "-s", "512x448", "-i", str(STREAM_FIFO),
                              "-f", "mjpeg", "-q:v", str(quality), "pipe:1"],
                             stdout=subprocess.PIPE, stdin=subprocess.DEVNULL)
        buf, n, t0 = b"", 0, time.time()
        while True:
            chunk = p.stdout.read1(1 << 16)
            if not chunk:
                break
            buf += chunk
            while True:
                a = buf.find(b"\xff\xd8")
                b = buf.find(b"\xff\xd9", a + 2) if a >= 0 else -1
                if a < 0 or b < 0:
                    break
                frame, buf = buf[a:b + 2], buf[b + 2:]
                n += 1
                now = time.time()
                if now - t0 >= 1:
                    video["fps"], n, t0 = n / (now - t0), 0, now
                with video["cond"]:
                    video["jpeg"] = frame
                    video["seq"] += 1
                    video["cond"].notify_all()
        p.wait()
        video["fps"] = None
        time.sleep(0.5)


def latest_frame(keep=20):
    fr = sorted((LIVE_DIR / "frames").glob("*.png"), key=lambda f: f.stat().st_mtime)
    for f in fr[:-keep]:
        f.unlink(missing_ok=True)
    return fr[-1] if fr else None


def gallery():
    out = []
    for d in GALLERY_DIRS:
        if d.is_dir():
            for f in d.glob("*.png"):
                st = f.stat()
                out.append({"name": f"{d.name}/{f.name}", "mtime": st.st_mtime, "size": st.st_size})
    return sorted(out, key=lambda x: -x["mtime"])[:160]


def git(*args, timeout=10):
    try:
        return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, timeout=timeout).stdout
    except Exception:
        return ""


def commits():
    out = git("log", "-40", "--format=%h|%cr|%an|%s", "pc-port")
    return [dict(zip(("hash", "when", "author", "subject"), l.split("|", 3))) for l in out.splitlines()]


_mcache = {"t": 0, "v": {}}


def metrics():
    """Slow-changing project metrics, cached for 30 s."""
    if time.time() - _mcache["t"] < 120:
        return _mcache["v"]
    v = {}
    v["port_commits"] = int((git("rev-list", "--count", f"{BASE_BRANCH}..pc-port").strip() or 0))
    ds = git("diff", "--shortstat", f"{BASE_BRANCH}...pc-port").strip()
    v["diffstat"] = ds
    v["asm_sites"] = v["asm_files"] = None
    try:  # sites with no host translation yet (translated ones keep their PS2 asm under #ifdef)
        rep = subprocess.run(["python3", "port/scripts/vu0_translate.py", "--report"], cwd=ROOT,
                             capture_output=True, text=True, timeout=60).stdout
        m = re.search(r"(\d+) untranslated", rep)
        if m:
            v["asm_sites"] = int(m.group(1))
            v["asm_files"] = len(re.findall(r"^src/\S+: \d+", rep, re.M))
    except Exception:
        pass
    v["c_files"] = len([l for l in git("ls-files", "src").splitlines() if l.endswith(".c")])
    stubs = ROOT / "port/build/autostubs.txt"
    v["autostubs"] = len(stubs.read_text().split()) if stubs.exists() else None
    if BUILD_BIN.exists():
        st = BUILD_BIN.stat()
        v["bin_size"], v["bin_mtime"] = st.st_size, st.st_mtime
    v["runtime_files"] = len(list((ROOT / "port/runtime").glob("*.c*")))
    v["shots_total"] = sum(len(list(d.glob("*.png"))) for d in GALLERY_DIRS if d.is_dir())
    docs = []
    for f in sorted(DOCS.glob("*.md")):
        heads = [l.lstrip("# ").strip() for l in f.read_text(errors="replace").splitlines() if l.startswith("## ")]
        docs.append({"name": f.name, "mtime": f.stat().st_mtime, "lines": f.read_text(errors="replace").count("\n"),
                     "heads": heads[:8]})
    v["docs"] = docs
    try:
        v["load"] = os.getloadavg()
    except OSError:
        v["load"] = None
    _mcache.update(t=time.time(), v=v)
    return v


def proc_info(pid):
    if not pid:
        return None
    try:
        st = Path(f"/proc/{pid}/status").read_text()
        rss = int(re.search(r"VmRSS:\s+(\d+)", st).group(1)) * 1024
        thr = int(re.search(r"Threads:\s+(\d+)", st).group(1))
        f = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
        cpu = (int(f[11]) + int(f[12])) / os.sysconf("SC_CLK_TCK")
        return {"rss": rss, "threads": thr, "cpu": cpu}
    except Exception:
        return None


PAGE = r"""<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0d1117"><meta name="color-scheme" content="dark">
<title>Xenosaga PC Port</title><style>
:root{--bg:#0d1117;--fg:#e6edf3;--mut:#8b949e;--card:#161b22;--line:#30363d;--acc:#58a6ff;--ok:#3fb950;--bad:#f85149;--warn:#d29922}
*{box-sizing:border-box}html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.5 system-ui,-apple-system,sans-serif;
padding:0 max(12px,env(safe-area-inset-right)) env(safe-area-inset-bottom) max(12px,env(safe-area-inset-left))}
header{padding:12px 0 8px;display:flex;flex-wrap:wrap;align-items:baseline;gap:4px 12px}
h1{margin:0;font-size:18px;line-height:1.25}.sub{color:var(--mut);font-size:13px}
.live{position:sticky;top:0;z-index:2;background:var(--bg);padding:8px 12px;margin:0 -12px}
#live{display:block;width:100%;aspect-ratio:8/7;object-fit:contain;background:#000;border-radius:8px;image-rendering:pixelated;cursor:zoom-in}
.bar{display:flex;align-items:center;gap:8px;font-size:13px;color:var(--mut);margin-top:6px;flex-wrap:wrap}
#mode{margin-left:auto;background:var(--card);color:var(--fg);border:1px solid var(--line);border-radius:999px;padding:8px 14px;font:inherit;font-size:13px;min-height:36px;cursor:pointer}
.dot{width:10px;height:10px;border-radius:50%;background:var(--mut);flex:none}.dot.on{background:var(--ok)}.dot.off{background:var(--bad)}
.tiles{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px;margin:10px 0}
.tile{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:10px 12px;min-width:0}
.tile b{display:block;font-size:20px;font-variant-numeric:tabular-nums;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.tile span{font-size:12px;color:var(--mut)}
details{background:var(--card);border:1px solid var(--line);border-radius:10px;margin:10px 0;min-width:0}
summary{list-style:none;cursor:pointer;padding:14px;min-height:48px;font-weight:600;display:flex;align-items:center}
summary::-webkit-details-marker{display:none}summary::after{content:"\25B8";color:var(--mut);transition:transform .15s}
details[open] summary::after{transform:rotate(90deg)}.body{padding:0 14px 14px}
.count{color:var(--mut);font-weight:400;font-size:13px;margin-left:auto;margin-right:8px}
ul{margin:0;padding:0;list-style:none}li{padding:9px 0;border-top:1px solid var(--line);font-size:14px;min-width:0}li:first-child{border-top:0}
code{color:var(--acc);font-size:13px}.mut{color:var(--mut);font-size:12px}
a{color:var(--acc)}.docs a{text-decoration:none;font-weight:600}
.heads{color:var(--mut);font-size:12px;margin-top:2px}
pre{white-space:pre-wrap;word-break:break-word;max-height:50vh;overflow:auto;font-size:12px;margin:0;-webkit-overflow-scrolling:touch}
table{width:100%;border-collapse:collapse;font-size:13px;font-variant-numeric:tabular-nums}
td,th{padding:5px 4px;border-top:1px solid var(--line);text-align:left}th{color:var(--mut);font-weight:500}
td.n{text-align:right}.barc{height:6px;background:var(--line);border-radius:3px;overflow:hidden}.barc i{display:block;height:100%;background:var(--acc)}
.gal{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}
.gal figure{margin:0;min-width:0}.gal img{display:block;width:100%;aspect-ratio:8/7;object-fit:cover;background:#000;border-radius:6px;cursor:zoom-in}
.gal figcaption{font-size:11px;color:var(--mut);overflow:hidden;text-overflow:ellipsis;white-space:nowrap;padding-top:2px}
#viewer{position:fixed;inset:0;background:rgba(0,0,0,.92);display:none;align-items:center;justify-content:center;z-index:9;padding:12px}
#viewer.on{display:flex}#viewer img{max-width:100%;max-height:85vh;image-rendering:pixelated}
#viewer p{position:absolute;bottom:max(16px,env(safe-area-inset-bottom));left:0;right:0;text-align:center;color:var(--mut);font-size:13px;margin:0}
.wide{display:none}
/* web controller: video + pad in one grid (portrait: pad under the video) */
#play{display:grid;grid-template-columns:1fr 1fr;grid-template-areas:"v v" "l r" "m m";gap:6px 10px;
 margin:0 0 4px;padding:0 0 8px}
#play .live{grid-area:v;position:static;margin:0;padding:8px 0 0}
.padL{grid-area:l}.padR{grid-area:r}.padM{grid-area:m}
.pad{touch-action:none;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none;display:flex;flex-direction:column;align-items:center;gap:8px}
.padM{flex-direction:row;justify-content:center;gap:12px}
.b{display:flex;align-items:center;justify-content:center;background:#21262d;border:1px solid var(--line);color:var(--fg);font-weight:600;cursor:pointer}
.b.on,.dpad i.on{background:var(--acc);color:#0d1117;border-color:var(--acc)}
.shoulders{display:flex;gap:8px;width:100%}.sh{flex:1;height:40px;border-radius:10px;font-size:14px}
.mid{height:34px;padding:0 14px;border-radius:999px;font-size:11px;letter-spacing:.05em;min-width:76px}
.dpad{position:relative;width:min(40vw,150px);aspect-ratio:1;cursor:pointer}
.dpad i{position:absolute;width:34%;height:34%;background:#21262d;border:1px solid var(--line)}
.dpad i.u{left:33%;top:0;border-radius:8px 8px 2px 2px}.dpad i.d{left:33%;bottom:0;border-radius:2px 2px 8px 8px}
.dpad i.l{top:33%;left:0;border-radius:8px 2px 2px 8px}.dpad i.r{top:33%;right:0;border-radius:2px 8px 8px 2px}
.dpad::after{content:"";position:absolute;left:33%;top:33%;width:34%;height:34%;background:#21262d}
.face{position:relative;width:min(40vw,150px);aspect-ratio:1}
.f{position:absolute;width:36%;height:36%;border-radius:50%;font-size:22px}
.tri{left:32%;top:0;color:#3fd0a0}.cr{left:32%;bottom:0;color:#7aa2ff}.sq{left:0;top:32%;color:#e88ad8}.ci{right:0;top:32%;color:#ff7b72}
.stick{position:relative;width:min(30vw,110px);aspect-ratio:1;border-radius:50%;background:#161b22;border:1px solid var(--line)}
.stick i{position:absolute;left:30%;top:30%;width:40%;height:40%;border-radius:50%;background:#30363d;border:1px solid var(--mut);pointer-events:none}
.stick.on i{background:var(--acc)}
#padinfo{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--mut);min-width:0;flex:1;justify-content:center;flex-wrap:wrap}
#held{color:var(--acc);font-family:ui-monospace,monospace;font-size:11px}
.pill{background:var(--card);color:var(--fg);border:1px solid var(--line);border-radius:999px;padding:8px 14px;font:inherit;font-size:13px;min-height:36px;cursor:pointer}
body.padding{overscroll-behavior:none}
/* phone landscape: controls on both sides of the video */
@media(orientation:landscape) and (max-height:560px){
 #play{grid-template-columns:1fr minmax(0,calc((100vh - 84px)*8/7)) 1fr;grid-template-areas:"l v r" "l m r";height:100vh;align-content:start;margin:0;padding:4px 0 0}
 #play .live{padding:0}#live{max-height:calc(100vh - 84px);width:auto;margin:0 auto}
 .bar{margin-top:2px;flex-wrap:nowrap;font-size:11px;gap:6px}#run{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
 .bar .pill,#mode{min-height:24px;padding:2px 8px;font-size:11px;white-space:nowrap}
 .mid{height:30px;min-width:64px}
 .padL,.padR{justify-content:center}.dpad,.face{width:min(22vw,40vh,150px)}.stick{width:min(16vw,30vh,100px)}
 header{display:none}
}
@media(min-width:720px){.dpad,.face{width:150px}.stick{width:110px}}
@media(min-width:720px){.tiles{grid-template-columns:repeat(4,minmax(0,1fr))}.gal{grid-template-columns:repeat(4,minmax(0,1fr))}}
/* desktop: dense dashboard, everything visible */
@media(min-width:1100px){
 body{font-size:14px;padding:0 16px 16px}
 .dash{display:grid;grid-template-columns:minmax(380px,1.1fr) minmax(0,1fr) minmax(0,1fr);gap:12px;align-items:start}
 .live{position:static;margin:0;padding:0}#play{position:static;margin:0;padding:0 0 12px}
 .tiles{grid-template-columns:repeat(3,minmax(0,1fr));gap:6px}.tile{padding:8px 10px}.tile b{font-size:17px}
 details{margin:0 0 12px}summary{min-height:0;padding:9px 12px;pointer-events:none}summary::after{display:none}
 .body{padding:0 12px 12px}li{padding:5px 0;font-size:13px}
 pre{max-height:none;height:420px}.gal{grid-template-columns:repeat(3,minmax(0,1fr));gap:6px}
 .scroll{max-height:440px;overflow:auto}.wide{display:block}
}
@media(min-width:1600px){.dash{grid-template-columns:minmax(440px,1.1fr) minmax(0,1fr) minmax(0,1fr) minmax(0,1fr)}.col4{display:block}}
</style></head><body>
<header><h1>Xenosaga Ep. I &mdash; PC port</h1><div class="sub" id="sub">loading&hellip;</div></header>
<div class="dash">
<div><div id="play"><div class="live"><img id="live" alt="Live game frame">
<div class="bar"><span class="dot" id="dot"></span><span id="run"></span>
<button id="restart" class="pill" type="button" hidden>Restart game</button>
<button id="mode" type="button">60 fps video</button></div></div>
<div class="padL pad">
 <div class="shoulders"><div class="b sh" data-b="L2">L2</div><div class="b sh" data-b="L1">L1</div></div>
 <div class="dpad" id="dpad" aria-label="D-pad"><i class="u"></i><i class="r"></i><i class="d"></i><i class="l"></i></div>
 <div class="stick" id="lstick" data-s="l" aria-label="Left stick"><i></i></div>
</div>
<div class="padR pad">
 <div class="shoulders"><div class="b sh" data-b="R1">R1</div><div class="b sh" data-b="R2">R2</div></div>
 <div class="face"><div class="b f tri" data-b="TRIANGLE">&#9651;</div><div class="b f sq" data-b="SQUARE">&#9633;</div>
 <div class="b f ci" data-b="CIRCLE">&#9675;</div><div class="b f cr" data-b="CROSS">&#10005;</div></div>
 <div class="stick" id="rstick" data-s="r" aria-label="Right stick"><i></i></div>
</div>
<div class="padM pad"><div class="b mid" data-b="SELECT">SELECT</div>
 <div id="padinfo"><span class="dot" id="pdot"></span><span id="pstat">pad: offline</span><span id="held"></span></div>
 <div class="b mid" data-b="START">START</div></div>
</div>
<div class="tiles" id="tiles"></div>
<details class="wide" open><summary>Live process</summary><div class="body"><table id="proc"></table></div></details>
<details class="wide" open><summary>Recent runs</summary><div class="body"><table id="hist"></table></div></details>
</div>
<div>
<details open><summary>Screenshots<span class="count" id="gc"></span></summary><div class="body scroll"><div class="gal" id="gal"></div></div></details>
<details><summary>Progress<span class="count" id="cc"></span></summary><div class="body scroll"><ul id="commits"></ul></div></details>
</div>
<div>
<details><summary>Game log<span class="count" id="lc"></span></summary><div class="body"><pre id="log"></pre></div></details>
<details class="wide" open><summary>Log activity by subsystem</summary><div class="body"><table id="tags"></table></div></details>
</div>
<div class="col4">
<details><summary>Stage docs<span class="count" id="dc"></span></summary><div class="body scroll"><ul class="docs" id="docs"></ul></div></details>
</div>
</div>
<div id="viewer"><img alt=""><p>tap to close</p></div>
<script>
const $=id=>document.getElementById(id);const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const fmtB=b=>b==null?'–':b>1048576?(b/1048576).toFixed(1)+' MB':(b/1024).toFixed(0)+' KB';
const ago=t=>{if(!t)return'–';const s=Date.now()/1000-t;return s<90?Math.round(s)+'s ago':s<5400?Math.round(s/60)+'m ago':Math.round(s/3600)+'h ago'};
const desk=matchMedia('(min-width:1100px)');function openAll(){if(desk.matches)document.querySelectorAll('details').forEach(d=>d.open=true)}
desk.addEventListener('change',openAll);openAll();
let busy=false,video=null,hasVideo=false;
try{video=localStorage.getItem('xsgVideo');}catch(e){}
function wantVideo(){return hasVideo&&video!=='0'&&!document.hidden}
function setMode(){const m=$('mode');m.hidden=!hasVideo;m.textContent=video==='0'?'Switch to 60 fps video':'Switch to snapshots';
 const L=$('live');if(wantVideo()){if(!L.src.includes('/live.mjpg'))L.src='/live.mjpg?'+Date.now();}else if(L.src.includes('/live.mjpg')){L.src='';tick();}}
$('mode').addEventListener('click',()=>{video=video==='0'?'1':'0';try{localStorage.setItem('xsgVideo',video)}catch(e){}setMode();});
$('live').addEventListener('error',()=>{if($('live').src.includes('/live.mjpg'))setTimeout(()=>{$('live').src='/live.mjpg?'+Date.now()},2000)});
function tick(){if(document.hidden||busy||wantVideo())return;busy=true;const i=new Image();
i.onload=()=>{$('live').src=i.src;busy=false};i.onerror=()=>{busy=false};i.src='/live.png?'+Date.now();}
function tile(v,l){return `<div class="tile"><b>${esc(v)}</b><span>${esc(l)}</span></div>`}
async function status(){if(document.hidden)return;try{const s=await (await fetch('/api/status')).json();
const r=s.runner,m=s.metrics;$('restart').hidden=!r.interactive;if(hasVideo!==!!r.video){hasVideo=!!r.video;setMode();}$('sub').textContent='branch pc-port · '+(m.diffstat||'')+' · '+new Date().toLocaleTimeString();
$('dot').className='dot '+(r.running?'on':'off');
$('run').textContent=r.running?`running · run #${r.runs} · ${Math.round(r.uptime)}s`:`stopped (exit ${r.exit})`;
$('tiles').innerHTML=[tile(r.frame??'–','game frame'),tile(r.fps?r.fps.toFixed(1):'–','frames/s'),
 tile(m.port_commits??'–','port commits'),tile(m.asm_sites??'–','asm untranslated'),
 tile(m.autostubs??'–','auto-stubbed syms'),tile(m.c_files??'–','game C files'),
 tile(fmtB(m.bin_size),'binary size'),tile(ago(m.bin_mtime),'last build'),tile(m.shots_total??'–','screenshots'),
 tile(r.proc?fmtB(r.proc.rss):'–','game RSS'),tile(m.load?m.load[0].toFixed(2):'–','host load'),tile(r.lines??0,'log lines')].join('');
const p=r.proc;$('proc').innerHTML=p?`<tr><th>PID</th><td class="n">${r.pid}</td></tr><tr><th>Threads</th><td class="n">${p.threads}</td></tr><tr><th>CPU time</th><td class="n">${p.cpu.toFixed(1)}s</td></tr><tr><th>RSS</th><td class="n">${fmtB(p.rss)}</td></tr><tr><th>Binary built</th><td class="n">${ago(r.bin_mtime)}</td></tr>`:'<tr><td class="mut">not running</td></tr>';
$('hist').innerHTML='<tr><th>Run</th><th>Exit</th><th class="n">Secs</th><th class="n">Frame</th><th class="n">Lines</th></tr>'+(r.history||[]).slice().reverse().map(h=>`<tr><td>#${h.run}</td><td style="color:${h.exit===0?'var(--ok)':'var(--bad)'}">${h.exit}</td><td class="n">${h.secs}</td><td class="n">${h.frame??'–'}</td><td class="n">${h.lines}</td></tr>`).join('');
const tg=Object.entries(r.tags||{}).sort((a,b)=>b[1]-a[1]).slice(0,16),mx=tg.length?tg[0][1]:1;
$('tags').innerHTML=tg.map(([k,v])=>`<tr><td><code>[${esc(k)}]</code></td><td class="n">${v}</td><td style="width:45%"><div class="barc"><i style="width:${100*v/mx}%"></i></div></td></tr>`).join('')||'<tr><td class="mut">no tagged lines yet</td></tr>';
const L=$('log');const atEnd=L.scrollTop+L.clientHeight>=L.scrollHeight-20;L.textContent=r.log.join('\n');if(atEnd)L.scrollTop=1e9;$('lc').textContent=r.log.length;
$('cc').textContent=s.commits.length;
$('commits').innerHTML=s.commits.map(c=>`<li><code>${esc(c.hash)}</code> ${esc(c.subject)} <span class="mut">${esc(c.when)}</span></li>`).join('');
$('dc').textContent=m.docs.length;
$('docs').innerHTML=m.docs.map(d=>`<li><a href="/doc/${encodeURIComponent(d.name)}">${esc(d.name)}</a> <span class="mut">${d.lines} lines · ${ago(d.mtime)}</span><div class="heads">${d.heads.map(esc).join(' · ')}</div></li>`).join('');
$('gc').textContent=s.gallery.length;
const g=s.gallery.map(x=>`<figure><img loading="lazy" data-full="/shot/${esc(x.name)}" src="/shot/${esc(x.name)}?${x.mtime}" alt="${esc(x.name)}"><figcaption>${esc(x.name.split('/').pop())} · ${ago(x.mtime)}</figcaption></figure>`).join('');
if($('gal').dataset.k!==g){$('gal').innerHTML=g;$('gal').dataset.k=g;}}catch(e){}}
function view(src){const v=$('viewer');v.querySelector('img').src=src;v.classList.add('on');}
$('gal').addEventListener('click',e=>{const im=e.target.closest('img');if(im)view(im.dataset.full);});
$('live').addEventListener('click',()=>view($('live').src));
$('viewer').addEventListener('click',()=>$('viewer').classList.remove('on'));
document.addEventListener('keydown',e=>{if(e.key==='Escape')$('viewer').classList.remove('on')});
document.addEventListener('visibilitychange',()=>{setMode();tick();status();});
tick();status();setInterval(tick,1000);setInterval(status,4000);
/* ---- web controller: touch (pointer events), keyboard, Gamepad API -> /ws/pad */
const BITS={SELECT:0x100,L3:0x200,R3:0x400,START:0x800,UP:0x1000,RIGHT:0x2000,DOWN:0x4000,LEFT:0x8000,
 L2:1,R2:2,L1:4,R1:8,TRIANGLE:0x10,CIRCLE:0x20,CROSS:0x40,SQUARE:0x80};
const SHORT={SELECT:'SEL',START:'STA',UP:'↑',RIGHT:'→',DOWN:'↓',LEFT:'←',TRIANGLE:'△',CIRCLE:'○',CROSS:'✕',SQUARE:'□'};
const ptr=new Map();            // touch pointerId -> {mask} or {stick:'l'|'r'}
const tst={l:null,r:null};      // touch stick positions [x,y] 0..255
let kmask=0,kst={l:[128,128],r:[128,128]},gmask=0,gst={l:[128,128],r:[128,128]};
let ws=null,wsOk=false,last='',lastSend=0,rtt=null;
function dpadMask(el,x,y){const r=el.getBoundingClientRect(),dx=(x-r.left)/r.width-.5,dy=(y-r.top)/r.height-.5;
 if(Math.hypot(dx,dy)<.1)return 0;const a=Math.atan2(dy,dx)*180/Math.PI;let m=0; // 8 sectors of 45 deg
 if(a>-67.5&&a<67.5)m|=BITS.RIGHT;if(a>22.5&&a<157.5)m|=BITS.DOWN;if(a>112.5||a<-112.5)m|=BITS.LEFT;if(a<-22.5&&a>-157.5)m|=BITS.UP;return m}
function hitMask(x,y){const el=document.elementFromPoint(x,y);if(!el)return 0;const d=el.closest('#dpad');if(d)return dpadMask(d,x,y);
 const b=el.closest('[data-b]');return b?BITS[b.dataset.b]:0}
function stickPos(el,x,y){const r=el.getBoundingClientRect(),R=r.width/2;let dx=(x-r.left-R)/R,dy=(y-r.top-R)/R;const l=Math.hypot(dx,dy);
 if(l>1){dx/=l;dy/=l}el.querySelector('i').style.transform=`translate(${dx*R*.75}px,${dy*R*.75}px)`;
 return [Math.round(128+dx*127),Math.round(128+dy*127)]}
function vib(){try{navigator.vibrate&&navigator.vibrate(12)}catch(e){}}
document.querySelectorAll('.pad').forEach(p=>{
 p.addEventListener('pointerdown',e=>{e.preventDefault();const s=e.target.closest('.stick');try{p.setPointerCapture(e.pointerId)}catch(_){}
  if(s){ptr.set(e.pointerId,{stick:s.dataset.s,el:s});s.classList.add('on');tst[s.dataset.s]=stickPos(s,e.clientX,e.clientY)}
  else{const m=hitMask(e.clientX,e.clientY);ptr.set(e.pointerId,{mask:m});if(m)vib()}update()});
 p.addEventListener('pointermove',e=>{const q=ptr.get(e.pointerId);if(!q)return;e.preventDefault();
  if(q.stick)tst[q.stick]=stickPos(q.el,e.clientX,e.clientY);else{const m=hitMask(e.clientX,e.clientY);if(m!==q.mask){if(m&~q.mask)vib();q.mask=m}}update()});
 const up=e=>{const q=ptr.get(e.pointerId);if(!q)return;ptr.delete(e.pointerId);
  if(q.stick){tst[q.stick]=null;q.el.classList.remove('on');q.el.querySelector('i').style.transform=''}update()};
 ['pointerup','pointercancel','lostpointercapture'].forEach(t=>p.addEventListener(t,up));
 p.addEventListener('contextmenu',e=>e.preventDefault());
 p.addEventListener('touchstart',e=>e.preventDefault(),{passive:false}); // no scroll / double-tap zoom on the pad
});
/* keyboard: the native defaults (STAGE10_INPUT.md) */
const KEYS={KeyW:'UP',ArrowUp:'UP',KeyS:'DOWN',ArrowDown:'DOWN',KeyA:'LEFT',ArrowLeft:'LEFT',KeyD:'RIGHT',ArrowRight:'RIGHT',
 KeyL:'CIRCLE',Space:'CIRCLE',KeyK:'CROSS',KeyJ:'SQUARE',KeyI:'TRIANGLE',KeyQ:'L1',KeyE:'R1',Digit1:'L2',Digit3:'R2',
 Enter:'START',NumpadEnter:'START',Backspace:'SELECT',KeyZ:'L3',KeyC:'R3'};
const RKEYS={Numpad8:[0,-1],Numpad5:[0,1],Numpad4:[-1,0],Numpad6:[1,0]};const rk=new Set();
function rstickKeys(){let x=0,y=0;rk.forEach(k=>{x+=RKEYS[k][0];y+=RKEYS[k][1]});kst.r=[128+Math.sign(x)*127,128+Math.sign(y)*127]}
function typing(e){const t=e.target;return t&&(t.tagName==='INPUT'||t.tagName==='TEXTAREA'||t.isContentEditable)}
addEventListener('keydown',e=>{if(typing(e)||e.ctrlKey||e.metaKey||e.altKey)return;const n=KEYS[e.code];
 if(n){e.preventDefault();kmask|=BITS[n];update()}else if(RKEYS[e.code]){e.preventDefault();rk.add(e.code);rstickKeys();update()}});
addEventListener('keyup',e=>{const n=KEYS[e.code];if(n){kmask&=~BITS[n];update()}else if(RKEYS[e.code]){rk.delete(e.code);rstickKeys();update()}});
addEventListener('blur',()=>{kmask=0;rk.clear();rstickKeys();ptr.clear();tst.l=tst.r=null;update()});
/* Gamepad API, standard mapping in PlayStation positions (south = CROSS, east = CIRCLE) */
const GPB=['CROSS','CIRCLE','SQUARE','TRIANGLE','L1','R1','L2','R2','SELECT','START','L3','R3','UP','DOWN','LEFT','RIGHT'];
let gpOn=false;
function pollGp(){let m=0,l=[128,128],r=[128,128],any=false;
 for(const g of (navigator.getGamepads?navigator.getGamepads():[])){if(!g||!g.connected)continue;any=true;
  g.buttons.forEach((b,i)=>{if(GPB[i]&&(b.pressed||b.value>.4))m|=BITS[GPB[i]]});
  const ax=v=>Math.abs(v)<.2?128:Math.max(0,Math.min(255,Math.round(128+v*127)));
  if(g.axes.length>=2&&(ax(g.axes[0])!==128||ax(g.axes[1])!==128))l=[ax(g.axes[0]),ax(g.axes[1])];
  if(g.axes.length>=4&&(ax(g.axes[2])!==128||ax(g.axes[3])!==128))r=[ax(g.axes[2]),ax(g.axes[3])]}
 gpOn=any;gmask=m;gst={l,r};update();if(any)requestAnimationFrame(pollGp)}
addEventListener('gamepadconnected',()=>{if(!gpOn)requestAnimationFrame(pollGp)});
const off=s=>s[0]!==128||s[1]!==128;
function stick(k){return tst[k]||(off(gst[k])?gst[k]:kst[k])}
function cur(){let m=kmask|gmask;ptr.forEach(q=>{if(q.mask)m|=q.mask});const l=stick('l'),r=stick('r');return {m,l,r}}
function show(m){document.querySelectorAll('.pad [data-b]').forEach(b=>b.classList.toggle('on',!!(m&BITS[b.dataset.b])));
 ['UP','RIGHT','DOWN','LEFT'].forEach((n,i)=>$('dpad').children[i].classList.toggle('on',!!(m&BITS[n])));
 $('held').textContent=Object.keys(BITS).filter(n=>m&BITS[n]).map(n=>SHORT[n]||n).join(' ')}
function update(force){const {m,l,r}=cur();const k=m+','+l+','+r;if(k!==last)show(m);
 if(!wsOk||(k===last&&!force))return;last=k;lastSend=performance.now();
 try{ws.send(JSON.stringify({buttons:m,lx:l[0],ly:l[1],rx:r[0],ry:r[1],t:Math.round(lastSend)}))}catch(e){}}
function pstat(){$('pdot').className='dot '+(wsOk?'on':'off');
 $('pstat').textContent=wsOk?('pad: connected'+(rtt!=null?` · ${rtt} ms`:'')+(gpOn?' · gamepad':'')):'pad: offline'}
function connect(){const u=(location.protocol==='https:'?'wss://':'ws://')+location.host+'/ws/pad';
 try{ws=new WebSocket(u)}catch(e){setTimeout(connect,2000);return}
 ws.onopen=()=>{wsOk=true;last='';update(true);pstat()};
 ws.onmessage=e=>{try{const d=JSON.parse(e.data);if(d.t!=null){rtt=Math.round(performance.now()-d.t);pstat()}}catch(_){}};
 ws.onclose=()=>{wsOk=false;pstat();setTimeout(connect,1000)};ws.onerror=()=>{try{ws.close()}catch(_){}}}
setInterval(()=>{if(wsOk&&performance.now()-lastSend>=190)update(true)},200); // keepalive (server drops silent clients after 500 ms)
connect();pstat();
$('restart').addEventListener('click',async()=>{if(!confirm('Restart the game?'))return;try{await fetch('/api/restart',{method:'POST'})}catch(e){}setTimeout(status,1500)});
</script></body></html>"""


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, code, body, ctype):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def file(self, p, ctype):
        try:
            self.send(200, p.read_bytes(), ctype)
        except OSError:
            self.send(404, b"not found", "text/plain")

    def ws_pad(self):
        key = self.headers.get("Sec-WebSocket-Key", "")
        if self.headers.get("Upgrade", "").lower() != "websocket" or not key:
            return self.send(400, b"websocket only", "text/plain")
        with pad["cond"]:
            if len(pad["clients"]) >= MAX_CLIENTS:
                return self.send(503, b"too many controllers", "text/plain")
            pad["next_id"] += 1
            cid = f"ws{pad['next_id']}"
        acc = base64.b64encode(hashlib.sha1(key.encode() + WS_GUID).digest()).decode()
        self.send_response(101)
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", acc)
        self.end_headers()
        self.wfile.flush()
        sock = self.connection
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.settimeout(30)
        self.close_connection = True
        win_t, win_n = time.monotonic(), 0
        try:
            ws_send(sock, json.dumps({"hello": cid, **pad_state()}))
            while True:
                fr = ws_recv(self.rfile)
                if fr is None:
                    break
                op, data = fr
                if op == 8:
                    ws_send(sock, b"", 8)
                    break
                if op == 9:
                    ws_send(sock, data, 10)
                    continue
                if op not in (1, 2):
                    continue
                now = time.monotonic()  # rate limit: > 250 messages in a second closes the socket
                if now - win_t >= 1:
                    win_t, win_n = now, 0
                win_n += 1
                if win_n > 250:
                    ws_send(sock, struct.pack(">H", 1008) + b"rate limit", 8)
                    break
                try:
                    d = json.loads(data)
                    pad_update(cid, d)
                except (ValueError, TypeError, AttributeError):
                    continue
                if "t" in d:  # echo for the page's round-trip time and the held indicator
                    ws_send(sock, json.dumps({"t": d["t"], **pad_state()}))
        except (OSError, ValueError):
            pass
        finally:
            pad_drop(cid)

    def do_POST(self):
        path = self.path.split("?", 1)[0]
        n = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(min(n, 4096)) if n else b""
        if path == "/api/pad":
            try:
                d = json.loads(body or b"{}")
                cid = "http:" + str(d.get("id") or self.client_address[0])[:64]
                pad_update(cid, d)
            except (ValueError, TypeError, AttributeError):
                return self.send(400, b"bad pad update", "text/plain")
            return self.send(200, json.dumps(pad_state()).encode(), "application/json")
        if path == "/api/restart":
            if not INTERACTIVE:
                return self.send(403, b"not in interactive mode", "text/plain")
            ok = restart_game()
            return self.send(200, json.dumps({"restarted": ok}).encode(), "application/json")
        self.send(404, b"not found", "text/plain")

    def do_GET(self):
        path = self.path.split("?", 1)[0]
        if path == "/ws/pad":
            return self.ws_pad()
        if path == "/":
            return self.send(200, PAGE.encode(), "text/html; charset=utf-8")
        if path == "/live.mjpg":
            if not video["ok"]:
                return self.send(404, b"no stream", "text/plain")
            self.send_response(200)
            self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Accel-Buffering", "no")
            self.end_headers()
            seq = -1
            try:
                while True:
                    with video["cond"]:
                        video["cond"].wait_for(lambda: video["seq"] != seq, timeout=5)
                        jpg, seq = video["jpeg"], video["seq"]
                    if jpg is None:
                        continue
                    self.wfile.write(b"--frame\r\nContent-Type: image/jpeg\r\nContent-Length: "
                                     + str(len(jpg)).encode() + b"\r\n\r\n" + jpg + b"\r\n")
                    self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                return
        if path == "/live.png":
            f = latest_frame()
            return self.file(f, "image/png") if f else self.send(404, b"no frame yet", "text/plain")
        if path.startswith("/shot/"):
            name = path[6:]
            for d in GALLERY_DIRS:
                if name.startswith(d.name + "/"):
                    p = (d / name[len(d.name) + 1:]).resolve()
                    if p.parent == d.resolve() and p.suffix == ".png":
                        return self.file(p, "image/png")
            return self.send(404, b"not found", "text/plain")
        if path.startswith("/doc/"):
            p = (DOCS / os.path.basename(path[5:])).resolve()
            if p.parent == DOCS.resolve() and p.suffix == ".md":
                return self.file(p, "text/plain; charset=utf-8")
            return self.send(404, b"not found", "text/plain")
        if path == "/api/status":
            with lock:
                r = {k: v for k, v in state.items() if k != "log"}
                r["log"] = list(state["log"][-150:])
                r["tags"] = dict(state["tags"])
                r["history"] = list(state["history"])
            r["uptime"] = time.time() - r["started"] if r["started"] and r["running"] else 0
            r["proc"] = proc_info(r["pid"])
            r["video_fps"] = video["fps"]
            r["video"] = video["ok"]
            r["interactive"] = INTERACTIVE
            r["pad"] = pad_state()
            body = {"runner": r, "commits": commits(), "gallery": gallery(), "metrics": metrics()}
            return self.send(200, json.dumps(body).encode(), "application/json")
        self.send(404, b"not found", "text/plain")


def main():
    global LIVE_DIR, STREAM_FIFO, PAD_FILE, BUILD_BIN, INTERACTIVE
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--no-runner", action="store_true")
    ap.add_argument("--shot-every", type=int, default=30, help="frames between live captures")
    ap.add_argument("--run-seconds", type=int, default=120, help="restart the game after this long")
    ap.add_argument("--pad", default="", help="[debug] pad script, e.g. 300:0x0800 (turns off --interactive)")
    ap.add_argument("--interactive", action="store_true",
                    help="no pad script or timeout; restart only on exit (default when no --pad)")
    ap.add_argument("--live-dir", default=str(LIVE_DIR), help="run folder (settings.ini, frames, pad.bin, FIFO)")
    ap.add_argument("--bin", default=str(BUILD_BIN), help="the xsg binary (gpu/xsg-gpu next to it)")
    ap.add_argument("--quality", type=int, default=5, help="MJPEG quality (2 best .. 31 worst)")
    a = ap.parse_args()
    LIVE_DIR = Path(a.live_dir).resolve()
    STREAM_FIFO, PAD_FILE = LIVE_DIR / "stream.fifo", LIVE_DIR / "pad.bin"
    BUILD_BIN = Path(a.bin).resolve()
    INTERACTIVE = a.interactive or not a.pad
    if a.interactive:
        a.pad = ""
    LIVE_DIR.mkdir(parents=True, exist_ok=True)
    threading.Thread(target=pad_writer, daemon=True).start()
    if shutil.which("ffmpeg") and not STREAM_FIFO.exists():
        os.mkfifo(STREAM_FIFO)
    threading.Thread(target=video_loop, args=(a.quality,), daemon=True).start()
    if not a.no_runner:
        threading.Thread(target=runner, args=(a.shot_every, a.run_seconds, a.pad), daemon=True).start()
    ThreadingHTTPServer.daemon_threads = True
    ThreadingHTTPServer(("127.0.0.1", a.port), H).serve_forever()


if __name__ == "__main__":
    main()
