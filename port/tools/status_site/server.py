#!/usr/bin/env python3
"""Xenosaga PC port status site: live screen, screenshot gallery, progress.

Stdlib only.  Runs a private copy of port/build/xsg in a loop with
XSG_SHOT_EVERY so the page can show the latest frame, and serves:
  /            the page
  /live.png    newest live frame
  /shot/<name> a gallery image
  /doc/<name>  a port/docs/*.md file (plain text)
  /api/status  JSON (commits, runner state, log tail, gallery list)

Usage: python3 port/tools/status_site/server.py [--port 8765] [--no-runner]
"""
import argparse, html, json, os, shutil, subprocess, threading, time
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BUILD_BIN = ROOT / "port/build/xsg"
LIVE_DIR = Path("/tmp/xsg-live")
GALLERY_DIRS = [ROOT / "port/docs/screenshots", ROOT / "port/build/shots"]
DOCS = ROOT / "port/docs"

state = {"running": False, "started": None, "runs": 0, "exit": None, "log": [], "bin_mtime": None}
lock = threading.Lock()


def runner(shot_every, run_seconds, pad):
    LIVE_DIR.mkdir(parents=True, exist_ok=True)
    while True:
        if not BUILD_BIN.exists():
            time.sleep(5); continue
        # private copy so rebuilds don't disturb the running instance
        exe = LIVE_DIR / "xsg"
        shutil.copy2(BUILD_BIN, exe)
        for f in LIVE_DIR.glob("*.png"):
            f.unlink(missing_ok=True)
        env = dict(os.environ, XSG_SHOT_EVERY=str(shot_every), XSG_SHOT_DIR=str(LIVE_DIR / "frames"),
                   XSG_MC_DIR=str(LIVE_DIR / "mc"), XSG_TIMEOUT=str(run_seconds))
        if pad:
            env["XSG_PAD"] = pad
        (LIVE_DIR / "frames").mkdir(exist_ok=True)
        with lock:
            state.update(running=True, started=time.time(), exit=None, log=[], bin_mtime=BUILD_BIN.stat().st_mtime)
            state["runs"] += 1
        p = subprocess.Popen([str(exe)], cwd=str(ROOT), env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, errors="replace")
        for line in p.stdout:
            with lock:
                state["log"].append(line.rstrip("\n"))
                del state["log"][:-200]
        rc = p.wait()
        with lock:
            state.update(running=False, exit=rc)
        time.sleep(3)


def prune_frames(keep=20):
    fr = sorted((LIVE_DIR / "frames").glob("*.png"), key=lambda f: f.stat().st_mtime)
    for f in fr[:-keep]:
        f.unlink(missing_ok=True)
    return fr[-1] if fr else None


def gallery():
    out = []
    for d in GALLERY_DIRS:
        if d.is_dir():
            for f in d.glob("*.png"):
                out.append({"name": f"{d.name}/{f.name}", "mtime": f.stat().st_mtime})
    return sorted(out, key=lambda x: -x["mtime"])[:120]


def commits():
    try:
        r = subprocess.run(["git", "log", "-25", "--format=%h|%cr|%s", "pc-port"], cwd=ROOT,
                           capture_output=True, text=True, timeout=10)
        return [dict(zip(("hash", "when", "subject"), l.split("|", 2))) for l in r.stdout.splitlines()]
    except Exception:
        return []


PAGE = """<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0d1117"><meta name="color-scheme" content="dark">
<title>Xenosaga PC Port</title><style>
:root{--bg:#0d1117;--fg:#e6edf3;--mut:#8b949e;--card:#161b22;--line:#30363d;--acc:#58a6ff;--ok:#3fb950;--bad:#f85149}
*{box-sizing:border-box}html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.5 system-ui,-apple-system,sans-serif;
padding:0 max(12px,env(safe-area-inset-right)) env(safe-area-inset-bottom) max(12px,env(safe-area-inset-left))}
header{padding:12px 0 8px}h1{margin:0;font-size:18px;line-height:1.25}
.sub{color:var(--mut);font-size:13px}
.live{position:sticky;top:0;z-index:2;background:var(--bg);padding:8px 0;margin:0 -12px;padding-left:12px;padding-right:12px}
#live{display:block;width:100%;aspect-ratio:8/7;object-fit:contain;background:#000;border-radius:8px;image-rendering:pixelated}
.bar{display:flex;align-items:center;gap:8px;font-size:13px;color:var(--mut);margin-top:6px}
.dot{width:10px;height:10px;border-radius:50%;background:var(--mut);flex:none}.dot.on{background:var(--ok)}.dot.off{background:var(--bad)}
details{background:var(--card);border:1px solid var(--line);border-radius:10px;margin:10px 0}
summary{list-style:none;cursor:pointer;padding:14px;min-height:48px;font-weight:600;display:flex;justify-content:space-between;align-items:center}
summary::-webkit-details-marker{display:none}summary::after{content:"\\25B8";color:var(--mut);transition:transform .15s}
details[open] summary::after{transform:rotate(90deg)}.body{padding:0 14px 14px}
.count{color:var(--mut);font-weight:400;font-size:13px;margin-left:auto;margin-right:8px}
ul{margin:0;padding:0;list-style:none}li{padding:10px 0;border-top:1px solid var(--line);font-size:14px}li:first-child{border-top:0}
code{color:var(--acc);font-size:13px}.mut{color:var(--mut);font-size:12px;display:block}
a{color:var(--acc)}.docs a{display:block;padding:12px 0;min-height:44px;text-decoration:none}
pre{white-space:pre-wrap;word-break:break-word;max-height:50vh;overflow:auto;font-size:12px;margin:0;-webkit-overflow-scrolling:touch}
.gal{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}
.gal figure{margin:0}.gal img{display:block;width:100%;aspect-ratio:8/7;object-fit:cover;background:#000;border-radius:6px}
.gal figcaption{font-size:11px;color:var(--mut);overflow:hidden;text-overflow:ellipsis;white-space:nowrap;padding-top:2px}
#viewer{position:fixed;inset:0;background:rgba(0,0,0,.92);display:none;align-items:center;justify-content:center;z-index:9;padding:12px}
#viewer.on{display:flex}#viewer img{max-width:100%;max-height:85vh;image-rendering:pixelated}
#viewer p{position:absolute;bottom:max(16px,env(safe-area-inset-bottom));left:0;right:0;text-align:center;color:var(--mut);font-size:13px;margin:0}
@media(min-width:720px){body{max-width:1200px;margin:0 auto}.gal{grid-template-columns:repeat(4,minmax(0,1fr))}}
@media(min-width:1000px){.wrap{display:grid;grid-template-columns:minmax(0,560px) minmax(0,1fr);gap:16px;align-items:start}.live{position:sticky;top:8px;margin:0;padding:0}}
</style></head><body>
<header><h1>Xenosaga Ep. I &mdash; PC port</h1><div class="sub" id="sub">loading&hellip;</div></header>
<div class="wrap"><div class="live"><img id="live" alt="Live game frame">
<div class="bar"><span class="dot" id="dot"></span><span id="run"></span></div></div>
<div>
<details open><summary>Screenshots<span class="count" id="gc"></span></summary><div class="body"><div class="gal" id="gal"></div></div></details>
<details><summary>Progress<span class="count" id="cc"></span></summary><div class="body"><ul id="commits"></ul></div></details>
<details><summary>Game log</summary><div class="body"><pre id="log"></pre></div></details>
<details><summary>Stage docs</summary><div class="body"><ul class="docs" id="docs"></ul></div></details>
</div></div>
<div id="viewer"><img alt=""><p>tap to close</p></div>
<script>
const $=id=>document.getElementById(id);const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
let busy=false;function tick(){if(document.hidden||busy)return;busy=true;const i=new Image();
i.onload=()=>{$('live').src=i.src;busy=false};i.onerror=()=>{busy=false};i.src='/live.png?'+Date.now();}
async function status(){if(document.hidden)return;try{const s=await (await fetch('/api/status')).json();
$('sub').textContent='branch pc-port \\u00b7 '+new Date().toLocaleTimeString();
const r=s.runner;$('dot').className='dot '+(r.running?'on':'off');
$('run').textContent=r.running?`running \\u00b7 run #${r.runs} \\u00b7 ${Math.round(r.uptime)}s`:`stopped (exit ${r.exit})`;
const L=$('log');const atEnd=L.scrollTop+L.clientHeight>=L.scrollHeight-20;L.textContent=r.log.join('\\n');if(atEnd)L.scrollTop=1e9;
$('cc').textContent=s.commits.length;
$('commits').innerHTML=s.commits.map(c=>`<li><code>${esc(c.hash)}</code> ${esc(c.subject)}<span class="mut">${esc(c.when)}</span></li>`).join('');
$('docs').innerHTML=s.docs.map(d=>`<li><a href="/doc/${encodeURIComponent(d)}">${esc(d)}</a></li>`).join('');
$('gc').textContent=s.gallery.length;
const g=s.gallery.map(x=>`<figure><img loading="lazy" data-full="/shot/${esc(x.name)}" src="/shot/${esc(x.name)}?${x.mtime}" alt="${esc(x.name)}"><figcaption>${esc(x.name.split('/').pop())}</figcaption></figure>`).join('');
if($('gal').dataset.k!==g){$('gal').innerHTML=g;$('gal').dataset.k=g;}}catch(e){}}
$('gal').addEventListener('click',e=>{const im=e.target.closest('img');if(!im)return;const v=$('viewer');v.querySelector('img').src=im.dataset.full;v.classList.add('on');});
$('viewer').addEventListener('click',()=>$('viewer').classList.remove('on'));
$('live').addEventListener('click',()=>{const v=$('viewer');v.querySelector('img').src=$('live').src;v.classList.add('on');});
document.addEventListener('visibilitychange',()=>{tick();status();});
tick();status();setInterval(tick,1000);setInterval(status,5000);
</script></body></html>"""


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, code, body, ctype, cache=False):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        if not cache:
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
        if path == "/live.png":
            f = prune_frames()
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
                r = dict(state, log=list(state["log"][-80:]))
            r["uptime"] = time.time() - r["started"] if r["started"] and r["running"] else 0
            body = {"runner": r, "commits": commits(), "gallery": gallery(),
                    "docs": sorted(f.name for f in DOCS.glob("*.md"))}
            return self.send(200, json.dumps(body).encode(), "application/json")
        self.send(404, b"not found", "text/plain")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--no-runner", action="store_true")
    ap.add_argument("--shot-every", type=int, default=30, help="frames between live captures")
    ap.add_argument("--run-seconds", type=int, default=120, help="restart the game after this long")
    ap.add_argument("--pad", default="", help="XSG_PAD script, e.g. 300:0x0800")
    a = ap.parse_args()
    if not a.no_runner:
        threading.Thread(target=runner, args=(a.shot_every, a.run_seconds, a.pad), daemon=True).start()
    ThreadingHTTPServer(("127.0.0.1", a.port), H).serve_forever()


if __name__ == "__main__":
    main()
