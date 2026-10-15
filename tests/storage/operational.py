#!/usr/bin/env python3
"""Physical syscall faults, complete backup restoration and long-run store checks."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tempfile

exe, faults = map(lambda s: Path(s).resolve(), sys.argv[1:3])
root = Path(tempfile.mkdtemp(prefix='rfc-operational-', dir=exe.parent))
checks = []
def call(mode, path, env=None):
    return subprocess.run([str(exe),mode,str(path)],env=env,capture_output=True,text=True,timeout=30)
base = root/'base'
assert call('seed',base).returncode == 0
original = json.loads(call('verify',base).stdout)
assert original == {'sequence':40}
for operation in ('write','fsync'):
    for error in ('ENOSPC','EIO'):
        for cut in range(1,7):
            target=root/f'{operation}-{error}-{cut}';shutil.copytree(base,target)
            env=dict(os.environ,LD_PRELOAD=str(faults),RFC_IO_FAULT_ROOT=str(target),RFC_IO_FAULT_OPERATION=operation,RFC_IO_FAULT_AFTER=str(cut),RFC_IO_FAULT_ERROR=error)
            result=call('append',target,env)
            injected='RFC injected physical I/O failure' in result.stderr
            if injected:assert result.returncode != 0,result.stdout
            observed=call('verify',target)
            assert observed.returncode == 0,observed.stderr
            sequence=json.loads(observed.stdout)['sequence']
            assert sequence in (40,41)
            restored=root/f'restored-{operation}-{error}-{cut}'
            shutil.copytree(target,restored)
            assert call('verify',restored).stdout==observed.stdout
            checks.append({'operation':operation,'error':error,'cut':cut,'injected':injected,'recovered_sequence':sequence})
assert sum(c['injected'] for c in checks)>=12
for n in range(100):
    observed=call('verify',base)
    assert observed.returncode==0 and json.loads(observed.stdout)==original
soak=root/'soak';shutil.copytree(base,soak)
result=call('soak',soak);assert result.returncode==0,result.stderr
(root/'RESULT.json').write_text(json.dumps({'fault_cases':checks,'reopens':100,'durable_append_reopens':100,'restore_cases':24,'root':str(root)},indent=2)+'\n')
print(f'{len(checks)} physical fault/restore cases; 100 reopen checks; 100 append/reopens: {root}')
