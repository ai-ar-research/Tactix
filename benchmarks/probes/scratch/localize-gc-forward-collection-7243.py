#!/usr/bin/env python3
from pathlib import Path
base = Path(__file__).with_name('localize-root-binary-collection-7236.py')
code = base.read_text().replace('root-binary-collection-7236-wave', 'gc-forward-collection-7243-wave').replace('20260911-7230-root-binary-profiles', '20260911-7241-gc-forward-profiles').replace('20260911-7231-root-binary-samples', '20260911-7242-gc-forward-samples')
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
