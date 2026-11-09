import re, subprocess, sys
# 3-way resolve conflict hunks in a splat yaml: per subsegment (kind,name) take the side that changed vs base
p = sys.argv[1]; base_ref = sys.argv[2]
base = subprocess.run(['git', 'show', f'{base_ref}:{p}'], capture_output=True, text=True).stdout
key = lambda l: (re.search(r', (\w+), ([\w/\-]+)\]', l) or re.search(r'type: (\.?\w+), vram: \S+ name: ([\w/\-]+)', l))
bmap = {}
for l in base.splitlines():
    k = key(l)
    if k: bmap[(k.group(1).lstrip('.'), k.group(2))] = l.strip()
def fix(m):
    a, b = m.group(1).splitlines(True), m.group(2).splitlines(True)
    da = {(key(l).group(1).lstrip('.'), key(l).group(2)): l for l in a if key(l)}
    db = {(key(l).group(1).lstrip('.'), key(l).group(2)): l for l in b if key(l)}
    out = []
    for l in a:
        k = key(l)
        if not k: out.append(l); continue
        kk = (k.group(1).lstrip('.'), k.group(2)); x, y = da[kk], db.get(kk, da[kk])
        if x.strip() == bmap.get(kk): out.append(y)
        elif y.strip() == bmap.get(kk) or x == y: out.append(x)
        else: print('BOTH CHANGED', kk, x.strip(), y.strip()); out.append(y)
    for l in b:
        k = key(l)
        if k and (k.group(1).lstrip('.'), k.group(2)) not in da: out.append(l)
    return ''.join(out)
s = open(p).read()
s = re.sub(r'<<<<<<< [^\n]*\n(.*?)=======\n(.*?)>>>>>>> [^\n]*\n', fix, s, flags=re.S)
open(p, 'w').write(s)
