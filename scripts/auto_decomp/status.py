#!/usr/bin/env python3
"""Live status page for the auto_decomp loop (read-only, stdlib only).

    python3 -B scripts/auto_decomp/status.py [--port 8787] [--host 127.0.0.1]

Serves one page that refreshes itself every 2 s from /api/status:
  - overall: ByteMatch measures from the objdiff report (build/report.json),
    queue size, matches in the last hour / day, an hourly match histogram;
  - workers: active leases (build/auto_decomp/claims) grouped by owner, with
    try_fn runs used (tries/), best score (bodies/*.best.json), lease age and
    the time since the body, a variant or the try counter last changed;
  - running permuter processes (permute_fn.py);
  - the latest loop.log events.
It only reads files the loop writes; it never takes a lock.
"""
from __future__ import annotations

import argparse
import collections
import datetime as dt
import json
import os
import re
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AD = ROOT / "build/auto_decomp"
TRY_BUDGET = 120

REPORT = ROOT / "build/report.json"
_rep = {"mtime": None, "data": None}


def report() -> dict | None:
    """ByteMatch measures from the objdiff report (build/report.json), cached by mtime."""
    try:
        mt = REPORT.stat().st_mtime
    except OSError:
        return None
    if _rep["mtime"] != mt:
        try:
            r = json.loads(REPORT.read_text())
        except (OSError, ValueError):
            return _rep["data"]
        keys = ("matched_code_percent", "matched_data_percent", "fuzzy_match_percent",
                "matched_functions", "total_functions", "complete_code_percent")
        _rep["data"] = {"at": mt, "measures": {k: r["measures"].get(k) for k in keys},
                        "units": [{"id": c["id"], "name": c.get("name", c["id"]),
                                   **{k: c["measures"].get(k) for k in keys}}
                                  for c in r.get("categories", [])]}
        _rep["mtime"] = mt
    return _rep["data"]


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


def permuters() -> list[dict]:
    out = []
    try:
        ps = subprocess.run(["ps", "-eo", "pid,etimes,args"], capture_output=True, text=True).stdout
    except OSError:
        return out
    # one permute_fn.py per job (permuter.py and its worker processes hang off it)
    for line in ps.splitlines()[1:]:
        pid, et, args = (line.strip().split(None, 2) + ["", ""])[:3]
        argv = args.split()
        if not any(a.endswith("permute_fn.py") for a in argv[:3]):
            continue
        opt = {argv[i]: argv[i + 1] for i in range(len(argv) - 1) if argv[i].startswith("--")}
        key = "/".join(opt.get(f"--{x}", "?") for x in ("unit", "tu", "name"))
        out.append({"pid": int(pid), "secs": int(et), "key": key})
    return out


def kind_history(per: int = 50) -> dict:
    """Latest PER MATCH / GIVEUP / FAIL lines from the WHOLE loop.log (the tail window
    used for the event feed is mostly SWEEP/PERMUTE lines). Cached by file size/mtime."""
    global _KH
    p = AD / "loop.log"
    try:
        st = p.stat()
    except OSError:
        return {}
    sig = (st.st_size, st.st_mtime)
    if _KH and _KH[0] == sig:
        return _KH[1]
    out = {}
    pat = re.compile(rb"^(\S+) (MATCH|GIVEUP|FAIL) (.*)$")
    with p.open("rb") as fh:
        hits = [m for m in (pat.match(l) for l in fh) if m]
    for m in reversed(hits):
        kind = m[2].decode()
        lst = out.setdefault(kind, [])
        if len(lst) < per:
            lst.append({"t": parse_ts(m[1].decode()), "kind": kind,
                        "msg": m[3].decode(errors="replace").rstrip(), "trend": None})
    _KH = (sig, out)
    return out


_KH = None


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
        # best of <name>.best.json and the variants' <name>.vN.best.json (workers score variant files)
        scores = [(read_json(f, {}) or {}).get("score") for f in
                  [body.with_name(f"{name}.best.json"), *body.parent.glob(f"{name}.v*.best.json")]]
        scores = [s for s in scores if isinstance(s, (int, float))]
        best = max(scores) if scores else None
        # last activity: the body, its variants (<name>.*), or the try counter (bumped by every try_fn run)
        mts = []
        for f in [body, AD / "tries" / k.replace("/", "__"), *body.parent.glob(f"{name}.*")]:
            try:
                mts.append(f.stat().st_mtime)
            except OSError:
                pass
        edited = now - max(mts) if mts else None
        workers[c.get("owner", "?")].append({
            "model": c.get("model") or "?",  # passed via next.py --model / claims.take(model=)
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
    trends, pbest, last = {}, {}, {}                       # SWEEP/PERMUTE score moves: "up" / "down"
    for i, line in enumerate(lines):
        rest = line.partition(" ")[2]
        kind, _, msg = rest.partition(" ")
        if kind not in ("SWEEP", "PERMUTE"):
            continue
        key = msg.split(" ", 1)[0]
        m = re.search(r"(\d\.\d+) -> (\d\.\d+)", msg)
        if m:                                    # "done 0.50 -> 0.51" / "pre-sweep a -> b"
            a, b = float(m[1]), float(m[2])
            if a != b:
                trends[i] = "up" if b > a else "down"
            last[key] = b
        elif kind == "SWEEP" and (m := re.search(r" -> (\d\.\d+) \(\d+w\)", msg)):
            b = float(m[1])                      # sweep step: only the new score is logged
            if key not in last:
                trends[i] = "base"
            elif b != last[key]:
                trends[i] = "up" if b > last[key] else "down"
            last[key] = b
        m = re.search(r"\bbest=(\d+)", msg)
        if m:                                    # permuter score: lower is better
            b = int(m[1])
            if key not in pbest:
                trends[i] = "base"               # first permuter score for this function
            elif b != pbest[key]:
                trends[i] = "up" if b < pbest[key] else "down"
            pbest[key] = b
    n = len(lines)
    for j, line in enumerate(lines[-40:][::-1]):
        ts, _, rest = line.partition(" ")
        kind, _, msg = rest.partition(" ")
        events.append({"t": parse_ts(ts), "kind": kind, "msg": msg,
                       "trend": trends.get(n - 1 - j)})
    # per-kind history: the filter buttons show the latest N of that kind from the
    # whole log window, not just whichever of them fall in the newest 40 lines
    by_kind = kind_history()

    perms = permuters()
    for w in workers.values():                   # a permuter lease: no try_fn budget in play
        for l in w:
            p = next((p for p in perms if p["key"] == l["key"]), None)
            if p is not None:
                l["permuter"] = p["secs"]
    ghidra = AD.parent / "ghidra/.stamp"
    return {
        "now": now,
        "report": report(),
        "queue": len(queue.get("candidates", [])),
        "matched_loop": state.get("matched_count"),
        "failed": len(state.get("failed") or {}),
        "match_1h": m1, "match_24h": m24, "giveup_24h": g24, "hourly": hourly,
        "workers": [{"owner": o, "leases": sorted(v, key=lambda x: x["age"])} for o, v in sorted(workers.items())],
        "events": events,
        "events_by_kind": by_kind,
        "permuters": perms,
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
@media (min-width:480px){.kpis{grid-template-columns:repeat(3,1fr)}}
.kpi{background:var(--panel2);border-radius:10px;padding:10px 12px}
.kpi .v{font-size:1.5rem;font-weight:650;line-height:1.15}
.kpi .l{color:var(--dim);font-size:.72rem}
.units{display:grid;gap:4px 18px;margin-top:16px;padding-top:14px;border-top:1px solid var(--line)}
.u{display:grid;grid-template-columns:4.4rem 1fr 3.4rem;align-items:center;gap:2px 10px;padding:4px 0;font-size:.82rem}
.u .n{font-family:var(--mono);font-weight:600}
.u .bar{height:6px}
.u .p{text-align:right;font-weight:600}
.u .c{grid-column:2/-1;color:var(--dim);font-size:.7rem;margin-top:-1px}
.u.off{opacity:.45}
@media (min-width:480px){.units{grid-template-columns:1fr 1fr}}
.hist{display:flex;align-items:flex-end;gap:2px;height:72px;margin-top:14px}
.hist i{flex:1;background:var(--acc);opacity:.85;border-radius:3px 3px 0 0;min-height:2px}
.hist i.z{opacity:.2}
.axis{display:flex;justify-content:space-between;color:var(--dim);font-size:.68rem;margin-top:4px}
.workers{display:grid;gap:8px;grid-template-columns:repeat(auto-fill,minmax(min(100%,460px),1fr))}
.w .kv{display:grid;grid-template-columns:repeat(4,auto);gap:2px 14px;margin-top:6px;font-size:.78rem;color:var(--dim);font-variant-numeric:tabular-nums}
.w .kv span b{display:block;color:var(--fg);font-weight:600;font-size:.9rem}
.w{min-width:0;background:var(--panel2);border-radius:9px;padding:7px 10px;border-left:3px solid var(--ok)}
.w.s-warn{border-left-color:var(--warn)}.w.s-bad{border-left-color:var(--bad)}
.w .t{display:flex;align-items:center;gap:6px}
.w .fn{min-width:0;flex:1 1 auto;font-family:var(--mono);font-weight:650;font-size:.9rem;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.w .loc{color:var(--dim);font-size:.75rem;font-family:var(--mono);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.w .md{font-size:.66rem;padding:1px 7px;border-radius:99px;background:var(--panel);color:var(--acc);white-space:nowrap;flex:none}
.w .md.local{color:var(--warn)}
.w .sub2{display:flex;justify-content:space-between;gap:8px;margin-top:3px;font-size:.72rem;color:var(--dim)}
.w .sub2 .loc{flex:1 1 auto;min-width:0}
.w .stats{white-space:nowrap;font-variant-numeric:tabular-nums}
.w .stats b{color:var(--fg);font-weight:600}
.w .bar{height:4px;margin-top:7px}
.chip{font-size:.64rem;padding:1px 7px;flex:none;border-radius:99px;border:1px solid currentColor;white-space:nowrap}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}.dim{color:var(--dim)}
.filters{display:flex;gap:6px;flex-wrap:wrap}
.filters button{font:inherit;font-size:.72rem;padding:4px 10px;border-radius:99px;border:1px solid var(--line);background:var(--panel2);color:var(--dim);cursor:pointer;text-transform:none;letter-spacing:0}
.filters button.on{color:var(--fg);border-color:var(--acc)}
.ev{max-height:420px;overflow:auto;display:grid;gap:2px;-webkit-overflow-scrolling:touch}
.e{display:grid;grid-template-columns:auto 1fr;gap:2px 10px;padding:7px 8px;border-radius:8px;font-size:.8rem}
.e:nth-child(odd){background:var(--panel2)}
.e .k{font-weight:700;font-size:.7rem;align-self:center}
.e .k .ar.up{color:var(--ok)}.e .k .ar.down{color:var(--bad)}.e .k .ar.base{color:var(--acc)}
.e .tm{color:var(--dim);font-size:.72rem;text-align:right;font-variant-numeric:tabular-nums}
.e .m{grid-column:1/-1;font-family:var(--mono);word-break:break-word;color:var(--fg);opacity:.9}
.empty{color:var(--dim);padding:18px 0;text-align:center;font-size:.85rem}
@media (min-width:720px){
  main{padding:20px;gap:16px;grid-template-columns:repeat(12,1fr)}
  .hero{grid-column:span 7}.loop{grid-column:span 5}.wk{grid-column:span 12}.evc{grid-column:span 12}
  .kpis{grid-template-columns:repeat(2,1fr)}
  .e{grid-template-columns:70px 120px 1fr}.e .m{grid-column:auto}.e .tm{order:-1;text-align:left}
}
</style></head><body>
<header><div class="hd"><h1>Xenosaga I decomp</h1><span class="live"><span class="dot" id="dot"></span><span id="upd">connecting…</span></span></div></header>
<main>
  <section class="card hero"><h2>Progress <span class="c" id="scope">ByteMatch · code</span></h2>
    <div class="row"><div class="big num" id="pct">–</div><div class="sub num" id="frac"></div></div>
    <div class="bar"><i id="pbar" style="width:0"></i></div>
    <div class="units" id="units"></div>
    <div id="cov" class="c" style="color:var(--dim);font-size:.75rem;margin-top:6px"></div>
  </section>
  <section class="card loop"><h2>Loop</h2>
    <div class="kpis">
      <div class="kpi"><div class="v num ok" id="mloop">–</div><div class="l">matched · loop</div></div>
      <div class="kpi"><div class="v num ok" id="m1">–</div><div class="l">matches · 1h</div></div>
      <div class="kpi"><div class="v num" id="m24">–</div><div class="l">matches · 24h</div></div>
      <div class="kpi"><div class="v num warn" id="g24">–</div><div class="l">give-ups · 24h</div></div>
      <div class="kpi"><div class="v num" id="q">–</div><div class="l">queued</div></div>
    </div>
    <div class="hist" id="hist"></div><div class="axis"><span>24h ago</span><span>12h</span><span>now</span></div>
  </section>
  <section class="card wk"><h2>Workers <span class="c" id="wc"></span></h2><div class="workers" id="workers"></div></section>
  <section class="card evc"><h2>Recent events
    <span class="filters" id="filters"><button data-k="" class="on">All</button><button data-k="MATCH">Match</button><button data-k="GIVEUP">Give-up</button><button data-k="FAIL">Fail</button></span></h2>
    <div class="ev" id="events"></div></section>
</main>
<script>
const $=id=>document.getElementById(id);
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const dur=s=>s==null?'–':s<60?Math.round(s)+'s':s<3600?Math.round(s/60)+'m':(s/3600).toFixed(1)+'h';
const KC={MATCH:'ok',GIVEUP:'warn',FAIL:'bad',LOST:'bad',PERMUTE:'dim'};
let last=0,filt='',data=null;
const when=t=>{if(!t)return'';const d=new Date(t*1000),n=new Date();const tm=d.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});
  return d.toDateString()===n.toDateString()?tm:d.toLocaleDateString([],{month:'short',day:'numeric'})+' '+tm};
$('filters').onclick=e=>{const b=e.target.closest('button');if(!b)return;filt=b.dataset.k;
  for(const x of $('filters').children)x.classList.toggle('on',x===b);renderEvents()};
function renderEvents(){if(!data)return;const ev=filt?((data.events_by_kind||{})[filt]||[]):data.events;
  $('events').innerHTML=ev.length?ev.map(e=>`<div class="e"><span class="k ${KC[e.kind]||'dim'}">${esc(e.kind)}${e.trend?` <span class="ar ${e.trend}">${{up:'▲',down:'▼',base:'●'}[e.trend]}</span>`:''}</span><span class="tm">${when(e.t)}</span><span class="m">${esc(e.msg)}</span></div>`).join(''):'<div class="empty">Nothing here.</div>'}
async function tick(){
  if(document.hidden)return;
  try{
    const d=data=await (await fetch('api/status',{cache:'no-store',signal:AbortSignal.timeout(8000)})).json(); last=Date.now();
    const r=d.report;
    if(r){
      const m=r.measures,pct=v=>(+v||0).toFixed(2)+'%';
      $('pct').textContent=pct(m.matched_code_percent);
      $('pbar').style.width=(+m.matched_code_percent||0)+'%';
      $('frac').innerHTML=`Functions ${Number(m.matched_functions).toLocaleString()} / ${Number(m.total_functions).toLocaleString()}<br>Data ${pct(m.matched_data_percent)} · fuzzy ${pct(m.fuzzy_match_percent)}`;
      $('units').innerHTML=r.units.map(u=>`<div class="u"><span class="n">${esc(u.name)}</span><div class="bar"><i style="width:${+u.matched_code_percent||0}%"></i></div><span class="p num">${(+u.matched_code_percent||0).toFixed(1)}%</span><span class="c num">${Number(u.matched_functions).toLocaleString()}/${Number(u.total_functions).toLocaleString()}</span></div>`).join('');
      $('cov').textContent=`report ${dur(Date.now()/1000-r.at)} old`;
    }else{$('pct').textContent='–';$('frac').textContent='no build/report.json yet';}
    $('mloop').textContent=Number(d.matched_loop||0).toLocaleString();
    $('m1').textContent=d.match_1h;$('m24').textContent=d.match_24h;$('g24').textContent=d.giveup_24h;$('q').textContent=d.queue.toLocaleString();
    const mx=Math.max(1,...d.hourly);$('hist').innerHTML=d.hourly.map((v,i)=>`<i class="${v?'':'z'}" style="height:${100*v/mx}%" title="${v} match${v==1?'':'es'}, ${23-i}h ago"></i>`).join('');
    const cards=[];let n=0;
    for(const w of d.workers)for(const l of w.leases){n++;
      const idle=l.edited;
      const st=l.permuter!=null?['permuting','ok']:l.ttl<300?['expiring','bad']:idle!=null&&idle>600?['idle','warn']:['working','ok'];
      const tp=Math.min(100,100*l.tries/d.try_budget);
      const loc='';
      cards.push(`<div class="w s-${st[1]}"><div class="t"><span class="fn" title="${esc(l.key)}">${esc(l.name)}</span><span class="md${loc}">${esc(l.model)}</span><span class="chip ${st[1]}">${st[0]}</span></div>
        <div class="sub2"><span class="loc" title="${esc(l.key)}">${esc(w.owner)} · ${esc(l.unit)}/${esc(l.tu)}${l.size?` · ${l.size} B`:''}</span></div>
        <div class="kv"><span>best<b>${l.best==null?'–':(+l.best).toFixed(4)}</b></span><span>tries<b>${l.tries} / ${d.try_budget}</b></span><span>running<b>${dur(l.age)}</b></span><span>last edit<b>${idle==null?'–':dur(idle)+' ago'}</b></span></div>
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
    srv = ThreadingHTTPServer((a.host, a.port), Handler)
    srv.daemon_threads = True
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
