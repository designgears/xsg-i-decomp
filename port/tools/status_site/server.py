#!/usr/bin/env python3
"""Xenosaga PC port status site: live screen, screenshots, progress, metrics.

Stdlib only.  Runs a private copy of port/build/xsg in a loop with
XSG_SHOT_EVERY so the page can show the latest frame, and serves:
  /            the page (mobile-first; dense dashboard on wide screens)
  /live.png    newest live frame
  /shot/<name> a gallery image
  /doc/<name>  a port/docs/*.md file (plain text)
  /api/status  JSON (runner, log, commits, gallery, metrics)

Usage: python3 port/tools/status_site/server.py [--port 8765] [--no-runner] [--pad 300:0x0800]
"""
import argparse, collections, json, os, re, shutil, subprocess, threading, time
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BUILD_BIN = ROOT / "port/build/xsg"
LIVE_DIR = Path("/tmp/xsg-live")
GALLERY_DIRS = [ROOT / "port/docs/screenshots", ROOT / "port/build/shots"]
DOCS = ROOT / "port/docs"
BASE_BRANCH = "decomp-work"

state = {"running": False, "started": None, "runs": 0, "exit": None, "log": [], "bin_mtime": None,
         "pid": None, "lines": 0, "tags": collections.Counter(), "frame": None, "frame_t": None,
         "fps": None, "history": []}
lock = threading.Lock()
FRAME_RE = re.compile(r"\bframe\s+(\d+)")
TAG_RE = re.compile(r"^\[([a-zA-Z0-9_]+)\]")


def runner(shot_every, run_seconds, pad):
    LIVE_DIR.mkdir(parents=True, exist_ok=True)
    while True:
        if not BUILD_BIN.exists():
            time.sleep(5); continue
        exe = LIVE_DIR / "xsg"  # private copy so rebuilds don't disturb the running instance
        shutil.copy2(BUILD_BIN, exe)
        frames = LIVE_DIR / "frames"
        frames.mkdir(exist_ok=True)
        for f in frames.glob("*.png"):
            f.unlink(missing_ok=True)
        env = dict(os.environ, XSG_SHOT_EVERY=str(shot_every), XSG_SHOT_DIR=str(frames),
                   XSG_MC_DIR=str(LIVE_DIR / "mc"), XSG_TIMEOUT=str(run_seconds))
        if pad:
            env["XSG_PAD"] = pad
        if STREAM_FIFO.exists():
            env["XSG_STREAM"] = str(STREAM_FIFO)
        p = subprocess.Popen([str(exe)], cwd=str(ROOT), env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, errors="replace")
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
        with lock:
            state.update(running=False, exit=rc, pid=None)
            state["history"].append({"run": state["runs"], "exit": rc, "secs": round(time.time() - state["started"]),
                                     "lines": state["lines"], "frame": state["frame"]})
            del state["history"][:-20]
        time.sleep(3)


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
@media(min-width:720px){.tiles{grid-template-columns:repeat(4,minmax(0,1fr))}.gal{grid-template-columns:repeat(4,minmax(0,1fr))}}
/* desktop: dense dashboard, everything visible */
@media(min-width:1100px){
 body{font-size:14px;padding:0 16px 16px}
 .dash{display:grid;grid-template-columns:minmax(380px,1.1fr) minmax(0,1fr) minmax(0,1fr);gap:12px;align-items:start}
 .live{position:static;margin:0;padding:0}
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
<div><div class="live"><img id="live" alt="Live game frame">
<div class="bar"><span class="dot" id="dot"></span><span id="run"></span>
<button id="mode" type="button">60 fps video</button></div></div>
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
const r=s.runner,m=s.metrics;if(hasVideo!==!!r.video){hasVideo=!!r.video;setMode();}$('sub').textContent='branch pc-port · '+(m.diffstat||'')+' · '+new Date().toLocaleTimeString();
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

    def do_GET(self):
        path = self.path.split("?", 1)[0]
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
            body = {"runner": r, "commits": commits(), "gallery": gallery(), "metrics": metrics()}
            return self.send(200, json.dumps(body).encode(), "application/json")
        self.send(404, b"not found", "text/plain")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--no-runner", action="store_true")
    ap.add_argument("--shot-every", type=int, default=30, help="frames between live captures")
    ap.add_argument("--run-seconds", type=int, default=120, help="restart the game after this long")
    ap.add_argument("--pad", default="", help="XSG_PAD script, e.g. 300:0x0800")
    ap.add_argument("--quality", type=int, default=5, help="MJPEG quality (2 best .. 31 worst)")
    a = ap.parse_args()
    LIVE_DIR.mkdir(parents=True, exist_ok=True)
    if shutil.which("ffmpeg") and not STREAM_FIFO.exists():
        os.mkfifo(STREAM_FIFO)
    threading.Thread(target=video_loop, args=(a.quality,), daemon=True).start()
    if not a.no_runner:
        threading.Thread(target=runner, args=(a.shot_every, a.run_seconds, a.pad), daemon=True).start()
    ThreadingHTTPServer(("127.0.0.1", a.port), H).serve_forever()


if __name__ == "__main__":
    main()
