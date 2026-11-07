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


PAGE = """<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Xenosaga PC Port</title><style>
:root{--bg:#0d1117;--fg:#e6edf3;--mut:#8b949e;--card:#161b22;--line:#30363d;--acc:#58a6ff}
body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.45 system-ui,sans-serif}
header{padding:14px 16px;border-bottom:1px solid var(--line)}h1{margin:0;font-size:18px}
main{display:grid;gap:16px;padding:16px;grid-template-columns:minmax(0,640px) minmax(0,1fr)}
@media(max-width:900px){main{grid-template-columns:1fr}}
.card{background:var(--card);border:1px solid var(--line);border-radius:8px;padding:12px}
h2{margin:0 0 8px;font-size:15px}#live{width:100%;image-rendering:pixelated;background:#000;border-radius:4px}
.mut{color:var(--mut)}pre{white-space:pre-wrap;word-break:break-all;max-height:300px;overflow:auto;font-size:12px;margin:0}
ul{margin:0;padding-left:18px}a{color:var(--acc)}.gal{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:8px}
.gal figure{margin:0}.gal img{width:100%;background:#000;border-radius:4px}.gal figcaption{font-size:11px;color:var(--mut);word-break:break-all}
code{color:var(--acc)}</style></head><body>
<header><h1>Xenosaga Episode I &mdash; native PC port</h1><div class="mut" id="sub"></div></header>
<main><section><div class="card"><h2>Live screen</h2><img id="live" alt="live frame"><div class="mut" id="run"></div></div>
<div class="card" style="margin-top:16px"><h2>Game log (tail)</h2><pre id="log"></pre></div></section>
<section><div class="card"><h2>Progress (pc-port commits)</h2><ul id="commits"></ul></div>
<div class="card" style="margin-top:16px"><h2>Stage docs</h2><ul id="docs"></ul></div>
<div class="card" style="margin-top:16px"><h2>Screenshots</h2><div class="gal" id="gal"></div></div></section></main>
<script>
const $=id=>document.getElementById(id);const esc=s=>s.replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
function tick(){$('live').src='/live.png?'+Date.now();}
async function status(){try{const s=await (await fetch('/api/status')).json();
$('sub').textContent='branch pc-port · updated '+new Date().toLocaleTimeString();
$('run').textContent=s.runner.running?`running (run #${s.runner.runs}, ${Math.round(s.runner.uptime)}s)`:`stopped, exit ${s.runner.exit}`;
$('log').textContent=s.runner.log.join('\\n');$('log').scrollTop=1e9;
$('commits').innerHTML=s.commits.map(c=>`<li><code>${c.hash}</code> ${esc(c.subject)} <span class="mut">${c.when}</span></li>`).join('');
$('docs').innerHTML=s.docs.map(d=>`<li><a href="/doc/${d}" target="_blank">${d}</a></li>`).join('');
const g=s.gallery.map(x=>`<figure><a href="/shot/${x.name}" target="_blank"><img loading="lazy" src="/shot/${x.name}?${x.mtime}"></a><figcaption>${esc(x.name)}</figcaption></figure>`).join('');
if($('gal').dataset.k!==g){$('gal').innerHTML=g;$('gal').dataset.k=g;}}catch(e){}}
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
