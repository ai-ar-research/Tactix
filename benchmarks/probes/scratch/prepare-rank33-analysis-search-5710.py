#!/usr/bin/env python3
"""Reject native Factor payloads before they exhaust the selected-window cap."""
from pathlib import Path
source=Path(__file__).with_name('prepare-rank33-analysis-window-5704.py').read_text()
source=source.replace('5704','5710')
source=source.replace('rank33-analysis-window','rank33-analysis-search')
anchor="    prefix + '_EVENT_TRACE_CONTROL_ONLY': '1',"
assert source.count(anchor)==1
# Existing SEARCH_ONLY rejects factor_*; CONTROL_ONLY alone admits them.
# Both selectors preserve conflict/analysis/learned/analyzed/score_bump events.
source=source.replace(anchor,anchor+"\n    prefix + '_EVENT_TRACE_SEARCH_ONLY': '1',")
source=source.replace("analysis_window=[182089,182093],", "analysis_window=[182089,182093],\n    observer_recovery='5704 native CONTROL_ONLY admits factor_* outside the detail window and saturated its100000-row cap at19378. Existing SEARCH_ONLY also enabled to reject these events before the cap.5704 remains retained;no solver source change.',")
exec(compile(source,__file__,'exec'))
