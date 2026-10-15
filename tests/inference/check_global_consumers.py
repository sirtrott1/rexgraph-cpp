#!/usr/bin/env python3
"""Small independent dense Fraction checks of the owning sparse action profile."""
import json
import subprocess
import sys
from fractions import Fraction as Q


def transpose(a):
    return list(map(list, zip(*a)))


def multiply(a, b):
    return [[sum((x*y for x,y in zip(row,col)), Q(0)) for col in transpose(b)] for row in a]


def apply(a, x):
    return [sum((v*t for v,t in zip(row,x)), Q(0)) for row in a]


def reduced(a):
    a=[list(row) for row in a]
    pivots=[]
    for j in range(len(a[0]) if a else 0):
        k=len(pivots)
        p=next((i for i in range(k,len(a)) if a[i][j]), None)
        if p is None:
            continue
        a[k],a[p]=a[p],a[k]
        scale=a[k][j]
        a[k]=[v/scale for v in a[k]]
        for i in range(len(a)):
            if i!=k:
                factor=a[i][j]
                a[i]=[v-factor*w for v,w in zip(a[i],a[k])]
        pivots.append(j)
        if len(pivots)==len(a):
            break
    return a,pivots


def solve(a, x):
    n=len(x)
    r,p=reduced([row+[v] for row,v in zip(a,x)])
    assert p==list(range(n)), 'singular independent reference'
    return [row[-1] for row in r]


def kernel(a, n):
    r,p=reduced(a)
    free=[j for j in range(n) if j not in p]
    f=[[Q(0)]*len(free) for _ in range(n)]
    for col,j in enumerate(free):
        f[j][col]=Q(1)
        for i,pivot in enumerate(p):
            f[pivot][col]=-r[i][j]
    return f


def inverse(a):
    n=len(a)
    return transpose([solve(a,[Q(i==j) for i in range(n)]) for j in range(n)])


cases=0
for line in subprocess.check_output([sys.argv[1],'--global-oracle'],text=True).splitlines():
    row=json.loads(line)
    k,kind=row['grade'],row['kind']
    metrics=[[[Q(v) for v in r] for r in m] for m in row['metrics']]
    boundaries=[None]+[[[Q(v) for v in r] for r in m] for m in row['boundaries']]
    x=list(map(Q,row['input']))
    n=len(x)
    l=[[Q(0)]*n for _ in range(n)]
    if k:
        b=boundaries[k]
        down=multiply(inverse(metrics[k]),multiply(transpose(b),multiply(metrics[k-1],b)))
        l=[[v+w for v,w in zip(a,b)] for a,b in zip(l,down)]
    if k<2:
        b=boundaries[k+1]
        up=multiply(b,multiply(inverse(metrics[k+1]),multiply(transpose(b),metrics[k])))
        l=[[v+w for v,w in zip(a,b)] for a,b in zip(l,up)]
    if kind==4:
        expected=apply(l,x)
    elif kind==6:
        lam=Q(row['lambda'])
        expected=solve([[Q(i==j)+lam*l[i][j] for j in range(n)] for i in range(n)],x)
    elif kind==7:
        f=kernel(l,n)
        expected=([Q(0)]*n if not f[0] else apply(f,solve(multiply(transpose(f),multiply(metrics[k],f)),apply(transpose(f),apply(metrics[k],x)))))
        assert apply(l,expected)==[Q(0)]*n
    else:
        raise AssertionError(kind)
    assert list(map(Q,row['output']))==expected,row
    cases+=1
assert cases==180,cases
print(json.dumps({'general_metric_green_projector_fraction_cases':cases}))
