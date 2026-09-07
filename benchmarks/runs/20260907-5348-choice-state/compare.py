#!/usr/bin/env python3
"""Validate observer neutrality and compare the selected pre-propagation state."""
import hashlib
import json
from pathlib import Path

C=Path(__file__).resolve().parent
summary=dict(stopped_clean=2,decisions_unchanged=True,states={},trace_sha256={})
for task,arm in enumerate(('tactix','kissat')):
    log=(C/'logs'/f'1668712_{task}.log').read_text()
    assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    raw=(C/'traces'/(arm+'.decisions.tsv')).read_bytes()
    rows=raw.splitlines()
    old=(C.parent/'20260907-5347-decision-stream'/'traces'/(arm+'.decisions.tsv')).read_bytes().splitlines()
    assert rows==[r for r in old if int(r.split(b'\t')[0])<=95801]
    prefix=[r.split('\t') for r in (C/'traces'/(arm+'.prefix.tsv')).read_text().splitlines() if not r.startswith('#')]
    assert len(prefix)==1 and prefix[0][1]=='95801' and int(prefix[0][4])==len(rows)
    state=(C/'traces'/(arm+'.state.tsv')).read_bytes()
    rs=[r.split('\t') for r in state.decode().splitlines()]
    summary['states'][arm]=dict(rows=len(rs),at_previous_decision=next(r for r in rs if r[1]=='253197'),at_split=next(r for r in rs if r[1]=='253198'))
    summary['trace_sha256'][arm]=dict(decisions=hashlib.sha256(raw).hexdigest(),state=hashlib.sha256(state).hexdigest())
summary['conclusion']='5416 is unassigned with equal score 9.25 in both arms before the split, but absent from Tactix score heap and present at native heap position 6926. Kissat then decides it; Tactix leaves it unassigned. Trace removal/liveness rather than preceding propagation first.'
print(json.dumps(summary,indent=2))
