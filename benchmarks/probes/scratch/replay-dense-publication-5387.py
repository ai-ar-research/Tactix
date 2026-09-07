#!/usr/bin/env python3
"""Reuse actual-object fixture driver with only immutable source/build substitutions."""
from pathlib import Path
root = Path(__file__).resolve().parents[3]
source = (root / 'benchmarks/probes/scratch/replay-vivify-retry-5378.py').read_text()
source = source.replace('build/pin-5377-retry-producers', 'build/pin-5378-vivify-retry')
source = source.replace("'build/pin-5378-vivify-retry', 0", "'build/pin-5387-dense-publication', 0")
source = source.replace('vivify-retry-5378', 'dense-publication-5387')
source = source.replace('20260907-5378-vivify-retry', '20260907-5387-dense-publication')
source = source.replace('kissat/src/vivify.c', 'kissat/src/clause.c')
source = source.replace('expected advice-fidelity failure', 'expected physical-publication mismatch')
exec(compile(source, str(root / 'benchmarks/probes/scratch/replay-dense-publication-5387.py'), 'exec'), {'__file__': __file__})
