#!/usr/bin/env python3
"""Live status page for the auto_decomp loop (read-only, stdlib only).

    python3 -B scripts/auto_decomp/status.py [--port 8787] [--host 127.0.0.1]

Serves one page that refreshes itself every 2 s from /api/status:
  - overall: per-unit progress (tools/coverage_report.py --json, cached 60 s),
    queue size, matches in the last hour / day, an hourly match histogram;
  - workers: active leases (build/auto_decomp/claims) grouped by owner, with
    try_fn runs used (tries/), best score (bodies/*.best.json), lease age and
    the time since the body, a variant or the try counter last changed;
  - the latest loop.log events.
It only reads files the loop writes; it never takes a lock.
"""
from __future__ import annotations

import argparse
import collections
import datetime as dt
import json
import os
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AD = ROOT / "build/auto_decomp"
TRY_BUDGET = 60

_cov = {"at": 0.0, "data": None}
_cov_lock = threading.Lock()


def coverage() -> dict | None:
    with _cov_lock:
        if time.time() - _cov["at"] > 60:
            try:
                p = subprocess.run([sys.executable, "-B", str(ROOT / "tools/coverage_report.py"), "--json"],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
                d = json.loads(p.stdout)
                _cov["data"] = [{k: r.get(k) for k in ("unit", "target", "functions", "functions_total", "gate")}
                                for r in d.get("rows", [])]
            except Exception:
                pass
            _cov["at"] = time.time()
        return _cov["data"]


def read_json(p: Path, default=None):
    try:
        return json.loads(p.read_text())
    except (OSError, ValueError):
        return default


def log_lines(n: int = 4000) -> list[str]:
    try:
        with (AD / "loop.log").open("rb") as fh:
            fh.seek(0, os.SEEK_END)
            fh.seek(max(0, fh.tell() - 400_000))
            return fh.read().decode(errors="replace").splitlines()[-n:]
    except OSError:
        return []


def parse_ts(s: str) -> float | None:
    try:
        return dt.datetime.strptime(s, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=dt.timezone.utc).timestamp()
    except ValueError:
        return None


def status() -> dict:
    now = time.time()
    queue = read_json(AD / "queue.json", {}) or {}
    sizes = {f"{c['unit']}/{c['tu']}/{c['name']}": c.get("size") for c in queue.get("candidates", [])}
    state = read_json(AD / "state.json", {}) or {}

    workers = collections.defaultdict(list)
    for p in sorted((AD / "claims").glob("*.json")):
        c = read_json(p)
        if not c or c.get("expires", 0) < now:
            continue
        k = c["key"]
        unit, tu, name = k.split("/", 2)
        body = AD / "bodies" / unit / tu / f"{name}.c"
        tries = read_json(AD / "tries" / k.replace("/", "__"), 0)
        best = (read_json(body.with_name(f"{name}.best.json"), {}) or {}).get("score")
        # last activity: the body, its variants (<name>.*), or the try counter (bumped by every try_fn run)
        mts = []
        for f in [body, AD / "tries" / k.replace("/", "__"), *body.parent.glob(f"{name}.*")]:
            try:
                mts.append(f.stat().st_mtime)
            except OSError:
                pass
        edited = now - max(mts) if mts else None
        workers[c.get("owner", "?")].append({
            "key": k, "unit": unit, "tu": tu, "name": name, "size": sizes.get(k),
            "age": now - c["at"], "ttl": c["expires"] - now, "tries": tries if isinstance(tries, int) else 0,
            "best": best, "edited": edited,
        })

    lines = log_lines()
    events, hourly = [], [0] * 24
    m1 = m24 = g24 = 0
    for line in lines:
        ts, _, rest = line.partition(" ")
        t = parse_ts(ts)
        kind = rest.split(" ", 1)[0]
        if t and kind == "MATCH":
            age = now - t
            if age < 3600:
                m1 += 1
            if age < 86400:
                m24 += 1
                hourly[23 - int(age // 3600)] += 1
        elif t and kind == "GIVEUP" and now - t < 86400:
            g24 += 1
    for line in lines[-40:][::-1]:
        ts, _, rest = line.partition(" ")
        kind, _, msg = rest.partition(" ")
        events.append({"t": parse_ts(ts), "kind": kind, "msg": msg})

    ghidra = AD.parent / "ghidra/.stamp"
    return {
        "now": now,
        "units": coverage(),
        "queue": len(queue.get("candidates", [])),
        "matched_loop": state.get("matched_count"),
        "failed": len(state.get("failed") or {}),
        "match_1h": m1, "match_24h": m24, "giveup_24h": g24, "hourly": hourly,
        "workers": [{"owner": o, "leases": sorted(v, key=lambda x: x["age"])} for o, v in sorted(workers.items())],
        "events": events,
        "ghidra_age": now - ghidra.stat().st_mtime if ghidra.is_file() else None,
        "try_budget": TRY_BUDGET,
    }


PAGE = r"""<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b0e14" media="(prefers-color-scheme:dark)">
<meta name="theme-color" content="#f5f7fa" media="(prefers-color-scheme:light)">
<title>xsg decomp</title>
<style>
:root{
  --bg:#0b0e14;--panel:#12161f;--panel2:#181d28;--line:#232a37;--fg:#e6edf3;--dim:#8590a2;--acc:#7aa2f7;--acc2:#bb9af7;
  --ok:#3fb950;--warn:#e3b341;--bad:#f85149;
  --mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;--r:14px;--gap:12px;
}
@media (prefers-color-scheme:light){:root{--bg:#f5f7fa;--panel:#fff;--panel2:#f0f3f7;--line:#dde3ea;--fg:#1f2328;--dim:#5f6b7a;--acc:#3b5bdb;--acc2:#8250df}}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,sans-serif;padding-bottom:env(safe-area-inset-bottom)}
header{position:sticky;top:0;z-index:5;background:color-mix(in srgb,var(--bg) 85%,transparent);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);border-bottom:1px solid var(--line);padding:calc(10px + env(safe-area-inset-top)) 16px 10px}
.hd{max-width:1200px;margin:0 auto;display:flex;align-items:center;gap:10px}
h1{font-size:1rem;margin:0;font-weight:650;flex:1;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.live{display:flex;align-items:center;gap:6px;font-size:.75rem;color:var(--dim)}
.dot{width:8px;height:8px;border-radius:50%;background:var(--ok);animation:pulse 2s infinite}
.dot.stale{background:var(--bad);animation:none}
@keyframes pulse{0%{box-shadow:0 0 0 0 #3fb95077}70%{box-shadow:0 0 0 7px #3fb95000}}
main{max-width:1200px;margin:0 auto;padding:var(--gap) 12px;display:grid;gap:var(--gap)}
.card{background:var(--panel);border:1px solid var(--line);border-radius:var(--r);padding:16px}
h2{font-size:.7rem;text-transform:uppercase;letter-spacing:.09em;color:var(--dim);margin:0 0 12px;font-weight:650;display:flex;justify-content:space-between;align-items:center;gap:8px}
h2 .c{text-transform:none;letter-spacing:0;font-weight:500}
.num{font-variant-numeric:tabular-nums}
.hero .big{font-size:clamp(2.4rem,9vw,3.4rem);font-weight:700;letter-spacing:-.03em;line-height:1;background:linear-gradient(90deg,var(--acc),var(--acc2));-webkit-background-clip:text;background-clip:text;color:transparent}
.hero .row{display:flex;align-items:flex-end;justify-content:space-between;gap:12px;flex-wrap:wrap}
.hero .sub{color:var(--dim);font-size:.85rem}
.bar{height:8px;background:var(--panel2);border-radius:99px;overflow:hidden}
.bar>i{display:block;height:100%;background:linear-gradient(90deg,var(--acc),var(--acc2));border-radius:99px;transition:width .5s ease}
.hero .bar{height:12px;margin-top:14px}
.kpis{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}
.kpi{background:var(--panel2);border-radius:10px;padding:10px 12px}
.kpi .v{font-size:1.5rem;font-weight:650;line-height:1.15}
.kpi .l{color:var(--dim);font-size:.72rem}
.units{display:grid;gap:12px}
.u .top{display:flex;justify-content:space-between;font-size:.85rem;margin-bottom:5px;gap:8px}
.u .n{font-family:var(--mono);font-weight:600}
.u .p{color:var(--dim)}
.u.off{opacity:.45}
.hist{display:flex;align-items:flex-end;gap:2px;height:72px;margin-top:14px}
.hist i{flex:1;background:var(--acc);opacity:.85;border-radius:3px 3px 0 0;min-height:2px}
.hist i.z{opacity:.2}
.axis{display:flex;justify-content:space-between;color:var(--dim);font-size:.68rem;margin-top:4px}
.workers{display:grid;gap:10px}
.w{background:var(--panel2);border-radius:12px;padding:12px;border-left:3px solid var(--ok)}
.w.s-warn{border-left-color:var(--warn)}.w.s-bad{border-left-color:var(--bad)}
.w .t{display:flex;justify-content:space-between;align-items:center;gap:8px}
.w .fn{font-family:var(--mono);font-weight:650;font-size:.9rem;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.w .loc{color:var(--dim);font-size:.75rem;font-family:var(--mono);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.w .stats{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-top:10px;font-size:.8rem}
.w .stats b{display:block;font-weight:600}
.w .stats span{color:var(--dim);font-size:.68rem}
.w .bar{height:5px;margin-top:10px}
.chip{font-size:.68rem;padding:2px 9px;border-radius:99px;border:1px solid currentColor;white-space:nowrap}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}.dim{color:var(--dim)}
.filters{display:flex;gap:6px;flex-wrap:wrap}
.filters button{font:inherit;font-size:.72rem;padding:4px 10px;border-radius:99px;border:1px solid var(--line);background:var(--panel2);color:var(--dim);cursor:pointer;text-transform:none;letter-spacing:0}
.filters button.on{color:var(--fg);border-color:var(--acc)}
.ev{max-height:420px;overflow:auto;display:grid;gap:2px;-webkit-overflow-scrolling:touch}
.e{display:grid;grid-template-columns:auto 1fr;gap:2px 10px;padding:7px 8px;border-radius:8px;font-size:.8rem}
.e:nth-child(odd){background:var(--panel2)}
.e .k{font-weight:700;font-size:.7rem;align-self:center}
.e .tm{color:var(--dim);font-size:.72rem;text-align:right;font-variant-numeric:tabular-nums}
.e .m{grid-column:1/-1;font-family:var(--mono);word-break:break-word;color:var(--fg);opacity:.9}
.empty{color:var(--dim);padding:18px 0;text-align:center;font-size:.85rem}
@media (min-width:720px){
  main{padding:20px;gap:16px;grid-template-columns:repeat(12,1fr)}
  .hero{grid-column:span 7}.loop{grid-column:span 5}.unitsc{grid-column:span 5}.wk{grid-column:span 7}.evc{grid-column:span 12}
  .kpis{grid-template-columns:repeat(2,1fr)}
  .e{grid-template-columns:70px 120px 1fr}.e .m{grid-column:auto}.e .tm{order:-1;text-align:left}
}
@media (min-width:1100px){.workers{grid-template-columns:1fr 1fr}}
</style></head><body>
<header><div class="hd"><h1>Xenosaga I decomp</h1><span class="live"><span class="dot" id="dot"></span><span id="upd">connecting…</span></span></div></header>
<main>
  <section class="card hero"><h2>Overall progress</h2>
    <div class="row"><div class="big num" id="pct">–</div><div class="sub num" id="frac"></div></div>
    <div class="bar"><i id="pbar" style="width:0"></i></div>
  </section>
  <section class="card loop"><h2>Loop <span class="c" id="aux"></span></h2>
    <div class="kpis">
      <div class="kpi"><div class="v num ok" id="m1">–</div><div class="l">matches · 1h</div></div>
      <div class="kpi"><div class="v num" id="m24">–</div><div class="l">matches · 24h</div></div>
      <div class="kpi"><div class="v num warn" id="g24">–</div><div class="l">give-ups · 24h</div></div>
      <div class="kpi"><div class="v num" id="q">–</div><div class="l">queued</div></div>
    </div>
    <div class="hist" id="hist"></div><div class="axis"><span>24h ago</span><span>12h</span><span>now</span></div>
  </section>
  <section class="card wk"><h2>Workers <span class="c" id="wc"></span></h2><div class="workers" id="workers"></div></section>
  <section class="card unitsc"><h2>Units</h2><div class="units" id="units"></div></section>
  <section class="card evc"><h2>Recent events
    <span class="filters" id="filters"><button data-k="" class="on">All</button><button data-k="MATCH">Match</button><button data-k="GIVEUP">Give-up</button><button data-k="FAIL">Fail</button></span></h2>
    <div class="ev" id="events"></div></section>
</main>
<script>
const $=id=>document.getElementById(id);
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const dur=s=>s==null?'–':s<60?Math.round(s)+'s':s<3600?Math.round(s/60)+'m':(s/3600).toFixed(1)+'h';
const KC={MATCH:'ok',GIVEUP:'warn',FAIL:'bad',LOST:'bad'};
let last=0,filt='',data=null;
const when=t=>{if(!t)return'';const d=new Date(t*1000),n=new Date();const tm=d.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});
  return d.toDateString()===n.toDateString()?tm:d.toLocaleDateString([],{month:'short',day:'numeric'})+' '+tm};
$('filters').onclick=e=>{const b=e.target.closest('button');if(!b)return;filt=b.dataset.k;
  for(const x of $('filters').children)x.classList.toggle('on',x===b);renderEvents()};
function renderEvents(){if(!data)return;const ev=data.events.filter(e=>!filt||e.kind===filt);
  $('events').innerHTML=ev.length?ev.map(e=>`<div class="e"><span class="k ${KC[e.kind]||'dim'}">${esc(e.kind)}</span><span class="tm">${when(e.t)}</span><span class="m">${esc(e.msg)}</span></div>`).join(''):'<div class="empty">Nothing here.</div>'}
async function tick(){
  if(document.hidden)return;
  try{
    const d=data=await (await fetch('api/status',{cache:'no-store'})).json(); last=Date.now();
    if(d.units){
      let f=0,t=0;
      $('units').innerHTML=d.units.map(u=>{f+=u.functions;t+=u.functions_total;const p=100*u.functions/u.functions_total;
        const g=u.gate==='passing'?'':u.gate==='not built'?' · not built':` · <span class="bad">gate ${esc(u.gate)}</span>`;
        return `<div class="u${u.gate==='not built'?' off':''}"><div class="top"><span class="n">${esc(u.unit)}</span><span class="p num">${u.functions}/${u.functions_total} · ${p.toFixed(1)}%${g}</span></div><div class="bar"><i style="width:${p}%"></i></div></div>`}).join('');
      $('pct').textContent=(100*f/t).toFixed(2)+'%';
      $('frac').innerHTML=`${f.toLocaleString()} / ${t.toLocaleString()} functions<br>${(t-f).toLocaleString()} to go`;
      $('pbar').style.width=(100*f/t)+'%';
    }
    $('m1').textContent=d.match_1h;$('m24').textContent=d.match_24h;$('g24').textContent=d.giveup_24h;$('q').textContent=d.queue.toLocaleString();
    const mx=Math.max(1,...d.hourly);$('hist').innerHTML=d.hourly.map((v,i)=>`<i class="${v?'':'z'}" style="height:${100*v/mx}%" title="${v} match${v==1?'':'es'}, ${23-i}h ago"></i>`).join('');
    $('aux').textContent=`ghidra ${dur(d.ghidra_age)} old`;
    const cards=[];let n=0;
    for(const w of d.workers)for(const l of w.leases){n++;
      const idle=l.edited;
      const st=l.ttl<300?['expiring','bad']:idle!=null&&idle>600?['idle','warn']:['working','ok'];
      const tp=Math.min(100,100*l.tries/d.try_budget);
      cards.push(`<div class="w s-${st[1]}"><div class="t"><span class="fn" title="${esc(l.key)}">${esc(l.name)}</span><span class="chip ${st[1]}">${st[0]}</span></div>
        <div class="loc">${esc(w.owner)} · ${esc(l.unit)}/${esc(l.tu)}${l.size?` · ${l.size}B`:''}</div>
        <div class="stats num"><div><b>${l.best==null?'–':(+l.best).toFixed(3)}</b><span>best</span></div><div><b>${l.tries}/${d.try_budget}</b><span>tries</span></div><div><b>${dur(l.age)}</b><span>leased</span></div><div><b>${dur(idle)}</b><span>idle</span></div></div>
        <div class="bar"><i style="width:${l.best==null?0:100*l.best}%"></i></div></div>`)}
    $('wc').textContent=n?`${n} active`:'';
    $('workers').innerHTML=cards.length?cards.join(''):'<div class="empty">No active leases.</div>';
    renderEvents();
  }catch(e){}
  upd();
}
function upd(){const age=(Date.now()-last)/1000;$('dot').className='dot'+(age>10?' stale':'');$('upd').textContent=last?(age>10?`offline ${dur(age)}`:'live'):'connecting…'}
document.addEventListener('visibilitychange',()=>{if(!document.hidden)tick()});
tick();setInterval(tick,2000);
</script></body></html>"""


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/api/status"):
            body, ctype = json.dumps(status()).encode(), "application/json"
        elif self.path in ("/", "/index.html"):
            body, ctype = PAGE.encode(), "text/html; charset=utf-8"
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *a):
        pass


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8787)
    a = ap.parse_args()
    print(f"status page: http://{a.host}:{a.port}/")
    ThreadingHTTPServer((a.host, a.port), Handler).serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
