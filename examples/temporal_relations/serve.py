#!/usr/bin/env python3
"""Serve one completed walkthrough with bounded native queries on loopback."""
import argparse
import importlib.util
from pathlib import Path

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location("native_temporal_server",HERE.parent/"temporal_dynamics/serve.py")
server=importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)
serve=server.serve

if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory",type=Path)
    parser.add_argument("--exe",type=Path,required=True)
    parser.add_argument("--port",type=int,default=0)
    args=parser.parse_args()
    serve(args.directory,args.exe,args.port)
