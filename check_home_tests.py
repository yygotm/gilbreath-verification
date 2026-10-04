"""Check the entire home test transcript against the bundled reference transcript.
Only elapsed seconds and the number of segments before SIGTERM may differ.
This reproduces existing tests; it is not an independent mathematical audit.
"""
from pathlib import Path
import difflib
import re
import sys

root = Path(__file__).resolve().parent
actual = (root / 'tests_home.out').read_text()
reference = (root / 'tests.out').read_text()
errors = []
if (root / 'tests_home.exit').read_text().strip() != '0':
    errors.append('test script exit code is not zero')
if not actual.rstrip().endswith('== done'):
    errors.append('test transcript is incomplete')
stopped = re.findall(r'^rc=9 DONE=(\d+) \(expect 9, fewer than 100\)$', actual, re.M)
if len(stopped) != 1 or not 0 <= int(stopped[0]) < 100:
    errors.append('SIGTERM segment count is absent or outside [0,100)')

def canonical(text):
    text = re.sub(r'time=\d+(?:\.\d+)?s', 'time=<elapsed>s', text)
    text = re.sub(r'^rc=9 DONE=\d+ \(expect 9, fewer than 100\)$',
                  'rc=9 DONE=<stopped> (expect 9, fewer than 100)', text, flags=re.M)
    return text.splitlines()

want, got = canonical(reference), canonical(actual)
if want != got:
    errors.append('reference transcript mismatch')
    print('\n'.join(difflib.unified_diff(want, got, fromfile='reference', tofile='home', lineterm='')))
if errors:
    print('HOME TESTS FAILED: ' + '; '.join(errors))
    sys.exit(1)
print('HOME TESTS VERIFIED: %d lines match reference, with elapsed seconds and bounded SIGTERM count excluded' % len(got))
print('G table, pi counts, SELFCHECK, checkpoint resume/extend, conflict/lock/K guards, split/merge, SIGTERM, records and negative tests reproduced')