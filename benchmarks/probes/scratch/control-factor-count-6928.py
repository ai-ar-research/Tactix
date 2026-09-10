#!/usr/bin/env python3
"""Correct the6927 nonexistent optional test path; retain its failure logs."""
from pathlib import Path
source = Path(__file__).with_name('control-factor-count-6927.py')
code = source.read_text()
assert '6927' in code and 'test_rule_instances' in code
code = code.replace('6927','6928').replace('test_rule_instances','test_kernel')
code = code.replace('Factor and rule-instance suites','Factor and kernel suites')
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
