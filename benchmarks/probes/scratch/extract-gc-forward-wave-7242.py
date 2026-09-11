#!/usr/bin/env python3
from pathlib import Path
base = Path(__file__).with_name('extract-root-binary-wave-7231.py')
code = base.read_text().replace('validate-root-binary-profiles-7230.py', 'validate-gc-forward-profiles-7241.py').replace('20260911-7231-root-binary-samples', '20260911-7242-gc-forward-samples')
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
