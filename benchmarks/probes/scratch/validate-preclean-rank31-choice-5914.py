#!/usr/bin/env python3
"""Replay-neutrality and full-heap comparison at the later Rank31 choice."""
from pathlib import Path
source=Path(__file__).with_name('validate-rank32-choice-5890.py')
code=source.read_text().replace('20260908-5883-rank32-choice-frontier','20260908-5910-preclean-rank31-choice')
code=code.replace("row[0] == '225987' and row[4] == '116746'", "row[0] == '394397' and row[4] == '87835'")
code=code.replace('through300K','through1M').replace('Heap snapshot follows decision61017804','Heap snapshot follows decision96579678')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
