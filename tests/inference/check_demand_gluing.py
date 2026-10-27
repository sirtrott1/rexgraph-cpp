"""Independent Fraction frontier and complete small-family reference."""
from fractions import Fraction as Q
from itertools import permutations,product
from pathlib import Path
import json,sys,hashlib,struct
root=Path(sys.argv[1]);edges={101:(1,2),102:(3,4),103:(2,3)}
def action(field,selected):
 out={}
 for edge in selected:
  a,b=edges[edge];difference=field.get(b,Q(0))-field.get(a,Q(0))
  out[a]=out.get(a,Q(0))-difference;out[b]=out.get(b,Q(0))+difference
 return {k:q for k,q in out.items() if q}
def moments(selected):
 x={1:Q(1)};out=[Q(1)]
 for _ in range(3):x=action(x,selected);out.append(x.get(1,Q(0)))
 return out
assert moments([101,102])==[1,1,2,4]
for order in permutations(edges):assert moments(order)==[1,1,2,5]
lines=(root/'observations.tsv').read_text().splitlines();assert [Q(v.split('\t')[1]) for v in lines]==[1,1,2,5]
w=action({1:Q(1)},edges);channels={e:(w.get(b,Q(0))-w.get(a,Q(0)))**2 for e,(a,b) in edges.items()};assert channels=={101:4,102:0,103:1}
# Native wire metadata remains original and names every inactive occurrence.
manifest=(root/'manifest.bin').read_bytes();cover=(root/'cover.bin').read_bytes();archive=(root/'source.archive').read_bytes()
assert struct.unpack_from('<QQ',cover)==(0x3156474458434652,1)
assert archive[40:40+len(manifest)]==manifest
booleans=[v for v in product([0,1],repeat=3) if v[0]+v[1]==1 and v[1]+v[2]==1];assert booleans==[(0,1,0),(1,0,1)]
assert not [v for v in product([0,1],repeat=3) if v[0]+v[1]==v[1]+v[2]==v[2]+v[0]==1]
report=dict(status='passed',moments_before=[1,1,2,4],moments_after=[1,1,2,5],assembly_orders=6,channels=channels,correlated_boolean_members=booleans,odd_cycle_boolean_members=0,original_sha256={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in ['source.archive','manifest.bin','cover.bin']})
(root/'reference-results.json').write_text(json.dumps(report,indent=2,default=str)+'\n');print('Independent demand gluing/moment/joint-family reference passed')
