#!/usr/bin/env python3
"""Combine the proven external observer with isolated candidate-only census."""
import importlib.util
import os
from pathlib import Path
import sys
assert os.environ.get('SLURM_JOB_ID'),'real solvers require SLURM'
c=Path(sys.argv[1]).resolve()
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module
protocol=load('runtime_protocol',c/'pins/runtime_task.py')
external=load('external_syscalls',c/'pins/syscall_memory_task.py')
proc=load('external_proc',c/'pins/proc_memory_task.py')
census=load('root_census',c/'pins/root_census_task.py')
for arm in ('candidate','previous'):
    binary=c/'pins'/arm
    proc.install_proc_observer(protocol,c,sys.argv[2],binary)
    external.install_observer(protocol,c,sys.argv[2],binary,c/'pins/strace')
census.install_observer(protocol,c,sys.argv[2],c/'pins/candidate')
raise SystemExit(protocol.main())
