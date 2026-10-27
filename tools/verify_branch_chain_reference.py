#!/usr/bin/env python3
"""Independent exact branch source/nullspace and declared number-field checks.

Consumes --reference output from rfc_branch_chain_tests. Uses only Fraction;
does not invoke the C++ recurrence builder, source residual or rank engine.
"""
import argparse
from fractions import Fraction as Q
import json
from pathlib import Path


def rref(matrix, cols):
    out=[list(row) for row in matrix]
    pivots=[]
    for col in range(cols):
        row=next((i for i in range(len(pivots),len(out)) if out[i][col]),None)
        if row is None:
            continue
        k=len(pivots)
        out[row],out[k]=out[k],out[row]
        a=out[k][col]
        out[k]=[x/a for x in out[k]]
        for i in range(len(out)):
            if i!=k and out[i][col]:
                a=out[i][col]
                out[i]=[x-a*y for x,y in zip(out[i],out[k])]
        pivots.append(col)
    return out,pivots


def product(a,b):
    return [[sum(x*y for x,y in zip(row,col)) for col in zip(*b)] for row in a]


class Field:
    def __init__(self,polynomial):
        self.p=list(map(Q,polynomial))
        assert len(self.p)>=2 and self.p[-1]==1
        self.d=len(self.p)-1
    def reduce(self,value):
        value=list(map(Q,value))
        while len(value)>self.d:
            q=value.pop()
            offset=len(value)-self.d
            for j in range(self.d):
                value[offset+j]-=q*self.p[j]
        return value+[Q(0)]*(self.d-len(value))
    def add(self,a,b):
        return self.reduce([x+y for x,y in zip(a,b)])
    def mul(self,a,b):
        out=[Q(0)]*(len(a)+len(b)-1)
        for i,x in enumerate(a):
            for j,y in enumerate(b):
                out[i+j]+=x*y
        return self.reduce(out)
    def scalar(self,x):
        return self.reduce([x])
    def evaluate(self,p,x):
        out=self.scalar(0)
        for q in reversed(p):
            out=self.add(self.mul(out,x),self.scalar(q))
        return out


def verify(cases):
    results=[]
    full=partial=modes_total=0
    original=set()
    for case in cases:
        shares=list(map(Q,case['shares']));m=len(shares);n=case['n'];count=n+m
        if (n,tuple(shares)) in [(10,(Q(1,2),Q(1,2))),(10,(Q(1,4),Q(3,4))),(9,(Q(1,3),Q(1,3),Q(1,3)))]:
            original.add((n,tuple(shares)))
        assert 0<n<=32 and 0<m<=8 and min(shares)>0 and sum(shares)==1
        assert len(case['occurrences'])==n*(m+1)
        boundary=[[Q(0)]*n for _ in range(count)]
        for row,col,q in case['occurrences']:
            assert 0<=row<count and 0<=col<n
            boundary[row][col]+=Q(q)
        expected=[[Q(0)]*n for _ in range(count)]
        for col in range(n):
            expected[col][col]=-1
            for j,share in enumerate(shares,1):
                expected[col+j][col]=share
        assert boundary==expected
        transposed=[list(row) for row in zip(*boundary)]
        reduced,pivots=rref(transposed,count)
        assert len(pivots)==n
        free=[j for j in range(count) if j not in pivots]
        independent=[[Q(0)]*m for _ in range(count)]
        for j,col in enumerate(free):
            independent[col][j]=1
            for i,pivot in enumerate(pivots):
                independent[pivot][j]=-reduced[i][col]
        basis=[list(map(Q,row)) for row in case['basis']]
        assert len(basis)==count and all(len(row)==m for row in basis)
        assert basis[:m]==[[Q(i==j) for j in range(m)] for i in range(m)]
        assert all(x==0 for row in product(transposed,basis) for x in row)
        assert len(rref([a+b for a,b in zip(basis,independent)],2*m)[1])==m
        modal=None
        if 'roots' in case:
            f=Field(case['field_polynomial']);roots=[f.reduce(r) for r in case['roots']]
            assert len({tuple(r) for r in roots})==len(roots) and 0<len(roots)<=m
            assert all(f.evaluate([Q(-1)]+shares,r)==f.scalar(0) for r in roots)
            columns=[]
            for root in roots:
                col=[];value=f.scalar(1)
                for i in range(count):
                    col.append(value);value=f.mul(value,root)
                columns.append(col)
            expected_modes=[list(row) for row in zip(*columns)]
            emitted=[[f.reduce(x) for x in row] for row in case['modes']]
            assert emitted==expected_modes
            for i in range(n):
                for j in range(len(roots)):
                    residual=f.scalar(0)
                    for k in range(count):
                        residual=f.add(residual,f.mul(f.scalar(boundary[k][i]),emitted[k][j]))
                    assert residual==f.scalar(0)
            # Independent rational rank of the regular-action blocks for the
            # initial-coordinate matrix proves independence over the field.
            regular=[[Q(0)]*(len(roots)*f.d) for _ in range(m*f.d)]
            for i in range(m):
                for j in range(len(roots)):
                    for k in range(f.d):
                        unit=[Q(l==k) for l in range(f.d)]
                        for l,q in enumerate(f.mul(emitted[i][j],unit)):
                            regular[i*f.d+l][j*f.d+k]=q
            rank=len(rref(regular,len(roots)*f.d)[1])
            assert rank==len(roots)*f.d
            complete=rank==m*f.d
            assert case['complete'] is complete
            full+=int(complete);partial+=int(not complete);modes_total+=len(roots)
            modal=dict(modes=len(roots),field_degree=f.d,complete=complete)
        results.append(dict(relations=n,shares=list(map(str,shares)),rank=n,nullity=m,modal=modal,passed=True))
    assert len(original)==3
    return dict(status='passed',cases=results,case_count=len(results),original_branch_examples=3,
                full_mode_families=full,partial_mode_families=partial,modes=modes_total,
                arithmetic='stdlib Fraction: original source rank/nullspace, independent algebraic quotient arithmetic and rational regular-block ranks')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('input',type=Path);p.add_argument('--output',type=Path)
    a=p.parse_args();out=verify(json.loads(a.input.read_text()))
    if a.output:a.output.write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps({k:v for k,v in out.items() if k!='cases'}))


if __name__=='__main__':main()
