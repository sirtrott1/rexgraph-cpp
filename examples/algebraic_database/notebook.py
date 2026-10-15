#!/usr/bin/env python3
"""Generate a native algebraic archive notebook from the C++ example sources."""
import argparse
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('output',type=Path)
args=parser.parse_args()
base=Path(__file__).resolve().parent
text='''%% cell 1 md
# Exact algebraic data

Create a native Q(i) database file, select its map records, and read a tensor window.
Each C++ process restores its own field and carrier owners. The second process
writes the same canonical archive and a separate JSON observation. Both cells
retain their outputs in the notebook artifact cache.

%% cell 2 cpp cache=shared out=measures.rfcxa
'''+(base/'seed.cpp').read_text()+'''
%% cell 3 cpp cache=shared in=measures.rfcxa out=query.json,measures-copy.rfcxa
'''+(base/'query.cpp').read_text()+'''
%% cell 4 display in=query.json
'''
args.output.parent.mkdir(parents=True,exist_ok=True)
with args.output.open('x') as file:file.write(text)
