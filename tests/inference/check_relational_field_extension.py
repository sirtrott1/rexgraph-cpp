"""Independent complete Fraction fields and original per-flag transformations."""
from fractions import Fraction as Q
import json
from pathlib import Path
import subprocess
import sys
import tempfile

def matrix(rows): return [[Q(x) for x in row] for row in rows]
def zeros(n,m): return [[Q(0)]*m for _ in range(n)]
def ident(n): return [[Q(i==j) for j in range(n)] for i in range(n)]
def transpose(a): return list(map(list,zip(*a))) if a else []
def mul(a,b): return [[sum((x*y for x,y in zip(row,col)),Q(0)) for col in zip(*b)] for row in a]
def act(a,x): return [sum((p*q for p,q in zip(row,x)),Q(0)) for row in a]
def add(a,b): return [[x+y for x,y in zip(row,other)] for row,other in zip(a,b)]
def neg(a): return [[-x for x in row] for row in a]
def inclusion(n): return ident(n)+[[Q(i+1,2) for i in range(n)]]
def back(n): return [row+[Q(0)] for row in ident(n)]
def grade_maps(dimensions,extra):
    n=sum(d+1 for d in dimensions)+extra;m=sum(dimensions);j=zeros(n,m);r=zeros(m,n);row=col=0
    for d in dimensions:
        for i,values in enumerate(inclusion(d)):
            for k,v in enumerate(values):j[row+i][col+k]=v
        for i in range(d):r[col+i][row+i]=1
        row+=d+1;col+=d
    return j,r
j1,r1=grade_maps([2,2],3);j0,r0=grade_maps([2,3,2],2)
k=zeros(9,5)
for col,row in enumerate([2,5,6,7,8]):k[row][col]=1
old_phi=map(matrix,[ [[0,0],[0,0]], [[1,0],[0,1],[1,1]], [[1,2],[0,1]], [[7,3],[2,5],[1,1]], [[1,0],[0,1]], [[2,1],[1,3]], [[0,0],[0,0],[0,0]], [[1,-1],[-2,2]] ])
old_phi=list(old_phi)
lowers=[0,1,0,1,2,0,1,2,0,1,3,3]
uppers=[0]*5+[1]*3+[2]*4
coeff=list(map(Q,[1,-1,-1,0,0,1,Q(1,2),-1,1,-1,Q(1,2),0]))
widths=[3,4,3,2];lower_offsets=[0,3,7,10];upper_offsets=[0,3,6]
expected_flags=[]
for slot in range(12):
    lower,upper=lowers[slot],uppers[slot];rows=widths[lower]
    if slot<8:
        n=[2,3,2][lower];ju=inclusion(2);ru=back(2);jl=inclusion(n)
        first=mul(mul(jl,old_phi[slot]),ru)
        extra=zeros(rows,3);extra[0][2]=Q(slot%3+1,3)
        phi=add(first,mul(extra,add(ident(3),neg(mul(ju,ru)))))
    else:
        phi=[[Q((row+1)*(col+2),slot-6) for col in range(3)] for row in range(rows)]
    expected_flags.append(phi)
b=zeros(12,9);occ=[]
for slot,phi in enumerate(expected_flags):
    source,target=upper_offsets[uppers[slot]],lower_offsets[lowers[slot]]
    for row,values in enumerate(phi):
        retained=[Q(0)]*9
        for col,v in enumerate(values):
            q=coeff[slot]*v;b[target+row][source+col]+=q;retained[source+col]=q
        occ.append(retained)
old_map=matrix([[1,0,1,0,0,0,0],[0,1,0,1,0,0,0],[0,0,0,0,1,1,0],[1,0,0,0,0,0,1]])
map3=mul(mul(j1,old_map),r0)
binary=str(Path(sys.argv[1]).resolve());cases=json.loads(subprocess.check_output([binary,'--json'],text=True));assert len(cases)==12
for case in cases:
    source=list(map(Q,case['source']));free=list(map(Q,case['free']))
    assert matrix(case['J'])==j1 and matrix(case['R'])==r1 and matrix(case['K'])==k
    assert mul(r1,j1)==ident(4) and mul(r1,k)==zeros(4,5)
    for slot,flag in enumerate(case['flags']):
        assert flag['slot']==slot and Q(flag['coefficient'])==coeff[slot]
        assert flag['source']==upper_offsets[uppers[slot]] and flag['target']==lower_offsets[lowers[slot]]
        assert matrix(flag['Phi'])==expected_flags[slot]
    fields={};directions={}
    for field in case['fields']:
        node=field['node'];action=matrix(field['map'])
        if not field['input']:
            particular=act(j1,source);direction=k
        else:
            expected={2:b,3:map3,99:ident(9)}[node];assert action==expected
            particular=act(action,fields[field['input']]);direction=mul(action,directions[field['input']])
        value=[p+d for p,d in zip(particular,act(direction,free))]
        assert particular==list(map(Q,field['particular'])) and direction==matrix(field['directions'])
        assert value==list(map(Q,field['value']))
        if 'occurrence_map' in field:
            assert matrix(field['occurrence_map'])==occ
            op=act(occ,fields[field['input']]);ok=mul(occ,directions[field['input']])
            assert op==list(map(Q,field['occurrence_particular'])) and ok==matrix(field['occurrence_directions'])
            assert [p+d for p,d in zip(op,act(ok,free))]==list(map(Q,field['occurrence_value']))
        fields[node]=particular;directions[node]=direction
    assert len(fields)==4 and act(r1,[p+d for p,d in zip(fields[1],act(k,free))])==source
print('12 independent complete Fraction field families/evaluations; 144 original flag transformations')

with tempfile.TemporaryDirectory(prefix='rexgraph-extension-independent-') as directory:
    folder=Path(directory);subprocess.check_call([binary,'--export',directory])
    assert sorted(p.name for p in folder.iterdir())==['authority.bin','extension.packet','local.packet']
    subprocess.check_call([binary,'--serve',str(folder/'extension.packet'),str(folder/'authority.bin')],cwd=directory)
    subprocess.check_call([binary,'--serve-local',str(folder/'local.packet'),str(folder/'authority.bin')],cwd=directory)
print('2 independent standalone extension/owning local packet processes, separate model file absent')
