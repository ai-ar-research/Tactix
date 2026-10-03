#!/usr/bin/env python3
"""Check complete CTest JUnit outcomes against the frozen test registry."""
import argparse
from collections import Counter
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def inspect(registry, junit):
    expected = [test['name'] for test in registry['tests']]
    if not expected or len(set(expected)) != len(expected):
        raise ValueError('empty or duplicate frozen registry')
    if junit.tag != 'testsuite':
        raise ValueError('expected one CTest testsuite')
    cases = junit.findall('testcase')
    names = [case.get('name') for case in cases]
    if Counter(names) != Counter(expected):
        raise ValueError('JUnit names do not match the frozen registry exactly')
    if any(case.get('status') not in ('run', 'fail') for case in cases):
        raise ValueError('JUnit contains a test that did not run')
    for case in cases:
        failure = case.find('failure') is not None or case.find('error') is not None
        if (case.get('status') == 'fail') != failure:
            raise ValueError('JUnit status disagrees with failure/error outcome')
    failed = [case.get('name') for case in cases if case.find('failure') is not None]
    errors = [case.get('name') for case in cases if case.find('error') is not None]
    skipped = [case.get('name') for case in cases if case.find('skipped') is not None]
    actual = dict(tests=len(cases), failures=len(failed), errors=len(errors),
                  skipped=len(skipped), disabled=0)
    for key, value in actual.items():
        text = junit.get(key, '0' if key == 'errors' else None)
        if text is None or not text.isdecimal() or int(text) != value:
            raise ValueError('JUnit summary disagrees with outcomes: ' + key)
    bad = set(failed + errors + skipped)
    return dict(registered=len(expected), executed=len(cases),
                passed=len(cases) - len(bad), failed=len(failed), errors=len(errors),
                skipped=len(skipped), disabled=0, failed_names=failed,
                error_names=errors, skipped_names=skipped,
                tests_accepted=not bad)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--registry', type=Path, required=True)
    parser.add_argument('--junit', type=Path, required=True)
    parser.add_argument('--ctest-exit', type=Path, required=True)
    parser.add_argument('--scheduler-state', required=True)
    parser.add_argument('--scheduler-exit', required=True)
    args = parser.parse_args()
    try:
        result = inspect(json.loads(args.registry.read_text()), ET.parse(args.junit).getroot())
        marker = args.ctest_exit.read_text().strip()
        result.update(ctest_exit=marker, scheduler_state=args.scheduler_state,
                      scheduler_exit=args.scheduler_exit)
        result['accepted'] = (result['tests_accepted'] and marker == '0' and
                              args.scheduler_state == 'COMPLETED' and
                              args.scheduler_exit == '0:0')
        # Pin/support preflight and postflight remain independent required gates.
        result['scope'] = 'Exact test completion and terminal exits only; not pin verification or whole-goal acceptance.'
    except (OSError, ValueError, KeyError, TypeError, ET.ParseError) as error:
        result = dict(accepted=False, error=str(error))
    print(json.dumps(result, indent=2))
    return 0 if result['accepted'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
