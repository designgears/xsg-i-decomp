import json,subprocess,sys
# union of TU entries: base(ours) + theirs; theirs wins for TUs it changed vs merge-base
def load(ref): return json.loads(subprocess.run(['git','show',f'{ref}:config/tu/data-carves.json'],capture_output=True,text=True,check=True).stdout)
base=load(sys.argv[1]); ours=load('HEAD'); theirs=load(sys.argv[2])
out=ours
for k,v in theirs['tus'].items():
    if base['tus'].get(k)!=v:
        if ours['tus'].get(k,base['tus'].get(k))!=base['tus'].get(k): print('BOTH CHANGED, taking theirs',k); out['tus'][k]=v; continue
        out['tus'][k]=v
json.dump(out,open('config/tu/data-carves.json','w'),indent=1); open('config/tu/data-carves.json','a').write('\n')
