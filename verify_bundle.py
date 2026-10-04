"""Read-only checks for saved evidence or explicitly supplied reproduction output."""
from pathlib import Path
import argparse
import hashlib
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
TAG = 'K=1535 B=8192 W=100000000000 T=128 sc=0 src=52c92c9f479a'
MILESTONES = ['x=1e11 pi=4118054813 G=417', 'x=1e12 pi=37607912018 G=481',
              'x=1e13 pi=346065536839 G=635', 'records checked: 26, mismatches: 0']

def need(ok, message):
    if not ok:
        raise ValueError(message)

def transcript(path):
    actual = path.read_text()
    reference = (ROOT / 'tests.out').read_text()
    stopped = re.findall(r'^rc=9 DONE=(\d+) \(expect 9, fewer than 100\)$', actual, re.M)
    need(len(stopped) == 1 and 0 <= int(stopped[0]) < 100, 'Invalid SIGTERM count')
    def normalize(value):
        value = re.sub(r'time=\d+(?:\.\d+)?s', 'time=<elapsed>s', value)
        value = re.sub(r'^rc=9 DONE=\d+ \(expect 9, fewer than 100\)$',
                       'rc=9 DONE=<stopped> (expect 9, fewer than 100)', value, flags=re.M)
        return value.splitlines()
    need(normalize(actual) == normalize(reference), 'Test transcript differs')
    print('REPRODUCED TEST TRANSCRIPT VERIFIED')

def check_run(prefix, saved=False):
    def file(suffix):
        return Path(str(prefix) + suffix)
    need(file('.exit').read_text().strip() == '0', 'Program exit was nonzero')
    need(file('.err').read_bytes() == b'', 'Program stderr not empty')
    lines = file('.log').read_text().splitlines()
    need(not any(s.startswith('FAIL ') for s in lines), 'FAIL line present')
    mask = (1 << 1536) - 1
    count, segments = 0, set()
    for line in lines:
        if not line.startswith('DONE '):
            continue
        head, payload = line.split('|', 1)
        fields = head.split(maxsplit=4)
        i, lo, hi = map(int, fields[1:4])
        need(fields[4] == TAG, 'Unexpected build or settings')
        need(i not in segments and 0 <= i < 100, 'Duplicate or invalid segment')
        need(lo == i * 10**11 and hi == (i + 1) * 10**11, 'Invalid endpoints')
        segments.add(i)
        kv = dict(s.split('=', 1) for s in payload.split() if '=' in s)
        need(kv['reason'] == 'none' and kv['sieve'] == 'cpu', 'Failed or non-CPU segment')
        hx = kv['clean']
        need(re.fullmatch('[0-9a-f]{384}', hx) is not None, 'Invalid clean mask')
        part = sum(int(hx[j:j+16], 16) << (4*j) for j in range(0, len(hx), 16))
        mask &= part
        count += int(kv['npos'])
    need(segments == set(range(100)), 'Incomplete coverage')
    g = next((k for k in range(1, 1536) if (mask >> k) & 1), None)
    need((count, g) == (346065536839, 635), 'pi or G differs')
    summary = 'ALLDONE X=10000000000000 ' + TAG + '| segs=100/100 pi=346065536839 G=635 conflicts=0 fail_lines=0 OK'
    need(lines[-1] == summary, 'Final summary differs')
    result = subprocess.run([sys.executable, str(ROOT / 'milestones.py'), str(file('.log')),
                             '1e11', '1e12', '1e13'], capture_output=True, text=True)
    need(result.returncode == 0 and result.stdout.splitlines() == MILESTONES,
         'Milestone reaggregation failed: ' + result.stdout + result.stderr)
    need(file('_milestones.out').read_text().splitlines() == MILESTONES, 'Saved milestone output differs')
    timing = file('.timing').read_text().strip()
    need(re.fullmatch(r'elapsed_seconds=\d+(?:\.\d+)?', timing) is not None, 'Invalid timing')
    elapsed = float(timing.split('=')[1])
    need(elapsed > 0, 'Invalid elapsed value')
    if saved:
        need(elapsed == 1135.578, 'Saved Bash timing differs')
        need(file('_runner.exit').read_text().strip() == '0', 'Runner exit differs')
        need('time=1135.952s' in file('.out').read_text(), 'Saved internal timing differs')
    print('RUN VERIFIED: 100 segments, pi=346065536839 G=635; ' + timing)

def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--tests', type=Path)
    group.add_argument('--run-prefix', type=Path)
    args = parser.parse_args()
    source = ROOT / 'gilbreath.c'
    need(hashlib.md5(source.read_bytes()).hexdigest() == '52c92c9f479a09643c43ca5038c4e4ca',
         'CPU source differs')
    if args.tests:
        transcript(args.tests)
        return
    if args.run_prefix:
        check_run(args.run_prefix)
        return
    checked = set()
    for line in (ROOT / 'SHA256SUMS').read_text().splitlines():
        expected, rel = line.split('  ', 1)
        path = (ROOT / rel).resolve()
        need(path.is_relative_to(ROOT) and rel not in checked, 'Unsafe or duplicate checksum path')
        checked.add(rel)
        need(hashlib.sha256(path.read_bytes()).hexdigest() == expected, 'Hash differs: ' + rel)
    actual = {p.relative_to(ROOT).as_posix() for p in ROOT.rglob('*') if p.is_file()
              and p.name != 'SHA256SUMS' and '.git' not in p.relative_to(ROOT).parts}
    need(checked == actual, 'Checksum coverage differs')
    need((ROOT / 'tests_home.exit').read_text().strip() == '0', 'Saved test exit differs')
    transcript(ROOT / 'tests_home.out')
    check_run(ROOT / 'prod/home_1e13', saved=True)
    print('BUNDLE VERIFIED: %d files hashed' % len(checked))

if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError, IndexError, StopIteration) as error:
        print('VERIFICATION FAILED: ' + str(error), file=sys.stderr)
        sys.exit(1)
