# 区間ごとの DONE 行から、節目 x（W の倍数）ごとに π(x) と G(π(x)) を出す。
# G(π(x)) は x 未満の全区間の「汚れなしの行」の集合の AND の最初の行。x 未満に欠けた区間があれば出さない。
# 受け付ける行: 'DONE ' で始まり改行で終わり、tag が 1 種類で、clean= がちょうど 16*(K//64+1) 桁の 16 進。
# 同じ区間に内容の違う行があれば止まる（ハードウェアの誤りの手がかり）。
# 使い方: python milestones.py logfile x1 x2 ...   （x は 1e15 や 1.5e15 のような書き方も可）
import sys
from decimal import Decimal

def parse(path):
    segs, tags, conflicts, fails = {}, set(), [], []
    for line in open(path):
        if line.startswith('FAIL '):
            f0 = line.split('|')[0].split()
            fails.append(f0[1] if len(f0) > 1 else '?')
        if not line.startswith('DONE ') or not line.endswith('\n') or '|' not in line:
            continue
        head, rest = line.split('|', 1)
        f = head.split()
        if len(f) < 5:
            continue
        i, s, e = int(f[1]), int(f[2]), int(f[3])
        tag = ' '.join(f[4:])
        kv = dict(x.split('=', 1) for x in rest.split() if '=' in x)
        tkv = dict(x.split('=', 1) for x in f[4:] if '=' in x)
        nhex = 16 * (int(tkv['K']) // 64 + 1)
        hx = kv.get('clean', '')
        if len(hx) != nhex or any(ch not in '0123456789abcdef' for ch in hx):
            continue                                   # 書きかけで切れた行は使わない
        W = int(tkv['W'])
        if s != i * W or e != s + W:
            continue
        mg = kv.get('maxgap', '0@0').split('@')
        rec = (s, e, int(kv['npos']), hx, int(mg[0]), int(mg[1]))
        if i in segs and segs[i] != rec:
            conflicts.append(i)
        segs.setdefault(i, rec)
        tags.add(tag)
    return segs, tags, conflicts, fails

def rows_mask(hx):
    # hex は 64 ビットの語を順に並べたもの（語 j の第 b ビットが行 64j+b）
    v = 0
    for j in range(len(hx) // 16):
        v |= int(hx[16 * j:16 * j + 16], 16) << (64 * j)
    return v

segs, tags, conflicts, fails = parse(sys.argv[1])
if fails:
    print('WARNING: %d FAIL lines in the log (segments %s); check reason= before publishing' % (len(fails), sorted(set(fails))[:10]))
if len(tags) != 1:
    sys.exit('expected exactly one tag in the log, found: %s' % sorted(tags))
if conflicts:
    sys.exit('conflicting DONE lines for segments %s' % sorted(set(conflicts))[:10])
tag = next(iter(tags))
W = int(dict(x.split('=', 1) for x in tag.split())['W'])
rc = 0
for xs in sys.argv[2:]:
    x = int(Decimal(xs))
    if x % W:
        print('x=%s: not a multiple of W=%d' % (xs, W)); rc = 1; continue
    n = x // W
    missing = [i for i in range(n) if i not in segs]
    if missing:
        print('x=%s: %d segments missing (first %d)' % (xs, len(missing), missing[0])); rc = 1; continue
    pi, mask, nbits = 0, -1, 0
    for i in range(n):
        s, e, np_, hx, _, _ = segs[i]
        mask &= rows_mask(hx); pi += np_; nbits = 4 * len(hx)
    G = next((k for k in range(1, nbits) if mask >> k & 1), None)
    if G is None:
        print('x=%s pi=%d G>K (K too small)' % (xs, pi)); rc = 1
    else:
        print('x=%s pi=%d G=%d' % (xs, pi, G))
# 記録的な素数の間隔（OEIS A002386 の始点、A005250 の大きさ。2026-10-05 に b-file で確認）と照合する。
# 検証済みの区間に入る記録はすべて、その区間の maxgap@p と一致し、記録より大きい差がそれより手前に無いこと。
RECORDS = [(436273009, 282), (1294268491, 288), (1453168141, 292), (2300942549, 320), (3842610773, 336), (4302407359, 354),
           (10726904659, 382), (20678048297, 384), (22367084959, 394), (25056082087, 456), (42652618343, 464), (127976334671, 468),
           (182226896239, 474), (241160624143, 486), (297501075799, 490), (303371455241, 500), (304599508537, 514), (416608695821, 516),
           (461690510011, 532), (614487453523, 534), (738832927927, 540), (1346294310749, 582), (1408695493609, 588), (1968188556461, 602),
           (2614941710599, 652), (7177162611713, 674), (13829048559701, 716), (19581334192423, 766), (42842283925351, 778),
           (90874329411493, 804), (171231342420521, 806), (218209405436543, 906), (1189459969825483, 916), (1686994940955803, 924),
           (1693182318746371, 1132)]
if segs:
    # 区間ごと: 中に記録があれば maxgap は中の最大の記録と一致。記録が無ければ maxgap はそれまでの最大の記録以下（記録の取りこぼしが無いこと）
    nck = bad = 0
    recmax = None                                             # これまでの最大の記録（最初の記録の区間より前は照合しない）
    for i in sorted(segs):
        if i > 0 and (i - 1) not in segs:
            break                                             # 連続して済んでいる区間だけ見る
        s, e, np_, hx, mg, mp = segs[i]
        inside = [(p, g) for (p, g) in RECORDS if s <= p < e]
        if inside:
            p, g = max(inside, key=lambda r: r[1])
            nck += len(inside)
            if not (mg == g and mp == p):
                bad += 1; print('RECORD MISMATCH: segment %d maxgap=%d@%d, expected %d@%d' % (i, mg, mp, g, p))
            recmax = g
        elif recmax is not None and mg > recmax:
            bad += 1; print('RECORD MISSING: segment %d maxgap=%d@%d exceeds the current record %d' % (i, mg, mp, recmax))
    print('records checked: %d, mismatches: %d' % (nck, bad))
    if bad:
        rc = 2
sys.exit(rc)
