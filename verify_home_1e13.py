from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parent
log = (root/'prod/home_1e13.log').read_text()
assert (root/'prod/home_1e13.exit').read_text().strip() == '0'
assert not re.search(r'^FAIL ', log, re.M), 'FAIL lines present'
expected_tag = 'K=1535 B=8192 W=100000000000 T=128 sc=0 src=52c92c9f479a'
segments = {}
for line in log.splitlines():
    if not line.startswith('DONE '):
        continue
    header, payload = line.split('|',1)
    pieces = header.split(maxsplit=4)
    i, lo, hi = map(int,pieces[1:4])
    assert pieces[4] == expected_tag, ('unexpected tag',pieces[4])
    assert i not in segments and 0 <= i < 100
    assert lo == i*100000000000 and hi == lo+100000000000
    fields = dict(item.split('=',1) for item in payload.split() if '=' in item)
    assert fields['reason'] == 'none' and fields['sieve']=='cpu', fields
    hx = fields['clean']
    assert re.fullmatch(r'[0-9a-f]{384}',hx)
    mask = sum(int(hx[j:j+16],16) << (4*j) for j in range(0,len(hx),16))
    segments[i] = (int(fields['npos']),mask)
assert set(segments) == set(range(100)), 'incomplete coverage'
pi_total = sum(v[0] for v in segments.values())
mask = (1<<1536)-1
for _,part in segments.values():
    mask &= part
G = next(k for k in range(1,1536) if mask>>k&1)
assert pi_total==346065536839 and G==635, (pi_total,G)
summary = 'ALLDONE X=10000000000000 '+expected_tag+'| segs=100/100 pi=346065536839 G=635 conflicts=0 fail_lines=0 OK'
assert log.splitlines()[-1] == summary, 'missing or unexpected final ALLDONE'
expected_milestones = [
'x=1e11 pi=4118054813 G=417',
'x=1e12 pi=37607912018 G=481',
'x=1e13 pi=346065536839 G=635',
'records checked: 26, mismatches: 0',
]
actual_milestones = (root/'prod/home_1e13_milestones.out').read_text().splitlines()
assert actual_milestones == expected_milestones, actual_milestones
print('HOME 1e13 VERIFIED: 100 unique contiguous CPU segments, pi=346065536839, G=635, FAIL=0, conflicts=0')
print('Milestones: G=417 at 1e11, G=481 at 1e12; 26 record prime gaps checked, mismatches=0')
print((root/'prod/home_1e13.timing').read_text().strip())