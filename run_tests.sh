#!/bin/bash
# 試験一式（コンテナ内で /w = このフォルダ）。結果は標準出力に要約を出す。
cd /w || exit 1
mkdir -p tlogs
H=$(md5sum gilbreath.c | cut -c1-12)
gcc -O3 -march=native -fopenmp -Wall -Wextra "-DSRC_HASH=\"$H\"" -o gilbreath gilbreath.c -lprimesieve 2> tlogs/build.txt || { cat tlogs/build.txt; exit 1; }
gcc -O3 -march=native -Wall -Wextra -o brute brute.c || exit 1
echo "== build warnings: $(wc -l < tlogs/build.txt) lines; src=$H"

echo "== G table: brute | T=0 (runs never split) | T=64 | T=4 + SELFCHECK, small blocks (expect Odlyzko: 5 15 35 65 95 135 175 248)"
for x in 100 1000 10000 100000 1000000 10000000 100000000 1000000000; do
  w=$x; [ $x -gt 1000000 ] && w=$((x/10))
  a=$(./brute $x 2000 2>/dev/null); [ $x -gt 100000000 ] && a="(skip)"
  b=$(./gilbreath $x 1024 4096 $w 0 | tail -1); c=$(./gilbreath $x 1024 4096 $w 64 | tail -1); d=$(SELFCHECK=1 ./gilbreath $x 400 37 $w 4 | tail -1)
  echo "$a | $b | $c | $d"
done
echo "== 1e10, 1e11 (expect G=329, 417; pi=455052511, 4118054813)"
./gilbreath 10000000000 1024 4096 1000000000 64 | tail -1
./gilbreath 100000000000 1024 4096 1000000000 64

echo "== SELFCHECK on [1e15, 1e15+2e9) (expect fail=none)"
SELFCHECK=1 BENCH_LO=1000000000000000 ./gilbreath 1000002000000000 1024 4096 100000000 64; echo "rc=$? (expect 4: bench run)"

echo "== checkpoint"
L=tlogs/ck.log; rm -f $L $L.*
./gilbreath 10000000000 1024 4096 1000000000 64 $L | tail -1; echo "rc=$? DONE=$(grep -c ^DONE $L) (expect 0, 10)"
./gilbreath 10000000000 1024 4096 1000000000 64 $L > /dev/null; echo "rerun rc=$? DONE=$(grep -c ^DONE $L) (expect 0, 10)"
./gilbreath 20000000000 1024 4096 1000000000 64 $L | tail -1; echo "extend rc=$? DONE=$(grep -c ^DONE $L) (expect 0, 20)"
python3 milestones.py $L 1e9 5e9 1e10 2e10 3e10; echo "milestones rc=$? (expect 1: 3e10 missing)"
echo "-- cut a DONE line in the middle of clean= (at a 16-digit boundary): recomputed, not used"
grep '^DONE' $L | grep -v '^DONE 13 ' > $L.cut; grep '^DONE 13 ' $L | sed -E 's/(clean=.{32}).*/\1/' | tr -d '\n' >> $L.cut
./gilbreath 20000000000 1024 4096 1000000000 64 $L.cut | tail -1; echo "rc=$? DONE 13 lines=$(grep -c '^DONE 13 ' $L.cut) (expect 0, 2)"
python3 milestones.py $L.cut 2e10; echo "rc=$? (expect 0, G=333)"
echo "-- conflicting duplicate line (one hex digit changed): ALLFAIL CONFLICT, rc 7; milestones refuses"
cp $L $L.conf; grep '^DONE 5 ' $L | head -1 | sed -E 's/1$/0/' >> $L.conf   # 最後の桁（行 1024 のビット）を変える
./gilbreath 20000000000 1024 4096 1000000000 64 $L.conf > $L.conf.out; echo "rc=$? $(tail -1 $L.conf.out | cut -c1-30) (expect 7, ALLFAIL ... CONFLICT)"
python3 milestones.py $L.conf 2e10; echo "rc=$? (expect nonzero)"
echo "-- lock: a second run on the same log must refuse (rc 8)"
rm -f $L.lk; (./gilbreath 50000000000 1024 4096 1000000000 64 $L.lk > /dev/null &) ; sleep 1
./gilbreath 50000000000 1024 4096 1000000000 64 $L.lk; echo "rc=$? (expect 8)"; wait; sleep 15
echo "-- K too small (K=50 at 1e6): FAIL line reason=K_too_small, rc 5"
rm -f $L.k; ./gilbreath 1000000 50 4096 100000 64 $L.k > $L.k.out; echo "rc=$? $(tail -1 $L.k.out) reason=$(grep -o 'reason=[a-z_A-Z]*' $L.k | sort | uniq -c | tr '\n' ' ')"
echo "-- RECHECK segments 3,7,19 into a separate log and compare with the main log"
rm -f $L.re; RECHECK=3,7,19 ./gilbreath 20000000000 1024 4096 1000000000 64 $L.re > /dev/null; echo "rc=$? (expect 4)"
for i in 3 7 19; do a=$(grep "^DONE $i " $L | sed -E 's/ rows=.* reason=/ /'); b=$(grep "^DONE $i " $L.re | sed -E 's/ rows=.* reason=/ /'); [ "$a" = "$b" ] && echo "seg $i SAME" || echo "seg $i DIFF"; done
echo "== SELFCHECK at production parameters (K=1535 B=8192 T=128) on [1e15, 1e15+2e9) and small X (expect fail=none / Odlyzko G)"
SELFCHECK=1 BENCH_LO=1000000000000000 ./gilbreath 1000002000000000 1535 8192 100000000 128 | head -1
for x in 1000000 100000000 10000000000; do SELFCHECK=1 ./gilbreath $x 1535 8192 $((x/10)) 128 | tail -1; done
echo "== SELFCHECK at production parameters near 1e16 (K=1535 B=8192 T=128), [1e16-2e9, 1e16) (expect fail=none, refblocks>0 likely)"
SELFCHECK=1 BENCH_LO=9999998000000000 ./gilbreath 10000000000000000 1535 8192 100000000 128 | head -1
echo "== SEG_LO/SEG_HI split into two logs, then merge (expect: each INCOMPLETE rc 7, merged ALLDONE rc 0, G=333)"
rm -f $L.p1 $L.p2 $L.merged*
SEG_LO=0 SEG_HI=10 ./gilbreath 20000000000 1024 4096 1000000000 64 $L.p1 > $L.p1.out; echo "part1 rc=$? $(tail -1 $L.p1.out | cut -c1-12) (expect 7 ALLFAIL)"
SEG_LO=10 SEG_HI=20 ./gilbreath 20000000000 1024 4096 1000000000 64 $L.p2 > $L.p2.out; echo "part2 rc=$? $(tail -1 $L.p2.out | cut -c1-12) (expect 7 ALLFAIL)"
cat $L.p1 $L.p2 > $L.merged; ./gilbreath 20000000000 1024 4096 1000000000 64 $L.merged > $L.merged.out; echo "merged rc=$? $(tail -1 $L.merged.out | cut -c1-140) (expect 0)"
echo "== SIGTERM: finish in-progress segments and exit 9; rerun completes (expect rc 9 then rc 0)"
rm -f $L.sig*; ./gilbreath 100000000000 1024 4096 1000000000 64 $L.sig > /dev/null & pid=$!; sleep 2; kill -TERM $pid; wait $pid; echo "rc=$? DONE=$(grep -c ^DONE $L.sig) (expect 9, fewer than 100)"
./gilbreath 100000000000 1024 4096 1000000000 64 $L.sig | tail -1; echo "rc=$? (expect 0)"
echo "== SIEVE_ONLY with BENCH_LO=0 must not look like a verification (expect rc 4)"
SIEVE_ONLY=1 BENCH_LO=0 ./gilbreath 1000000 1024 4096 100000 64 > /dev/null; echo "rc=$?"
echo "== segment start on a prime (W=3, W=5), K boundary (K=5 ok, K=4 rc 5), tiny B with word boundaries (SELFCHECK T=1)"
./gilbreath 30 1024 4096 3 0 | tail -1; ./gilbreath 100 1024 4096 5 0 | tail -1   # expect pi=10 G=3 / pi=25 G=5 (W < 4096 may have segments without primes)
./gilbreath 100 5 4096 100 64 > $L.k5; echo "K=5 rc=$? $(tail -1 $L.k5) (expect 0, G=5)"; ./gilbreath 100 4 4096 100 64 > $L.k4; echo "K=4 rc=$? $(tail -1 $L.k4) (expect 5)"
for b in 1 2 63 64 65; do echo -n "B=$b: "; SELFCHECK=1 ./gilbreath 1000000 1024 $b 100000 1 | tail -1; done
echo "== RECHECK exit code (expect 4)"; RECHECK=3 ./gilbreath 20000000000 1024 4096 1000000000 64 $L.re2 > /dev/null; echo "rc=$?"
echo "== record prime gaps (OEIS A002386/A005250) up to 1e12 with production parameters (expect 21 records checked, 0 mismatches; G=481)"
rm -f $L.rec; ./gilbreath 1000000000000 1535 8192 10000000000 128 $L.rec | tail -1; python3 milestones.py $L.rec 1e11 1e12; echo "rc=$?"
echo "-- negative: change the record segment 73 (540@738832927927) to 538, and segment 99 (no record) to 1200 (expect RECORD MISMATCH rc 2, RECORD MISSING rc 2)"
sed -E '/^DONE 73 /s/maxgap=540@/maxgap=538@/' $L.rec > $L.rec.mm; python3 milestones.py $L.rec.mm 1e12 | grep -E 'RECORD|records'; echo "rc=${PIPESTATUS[0]}"
sed -E '/^DONE 99 /s/maxgap=[0-9]+@/maxgap=1200@/' $L.rec > $L.rec.ms; python3 milestones.py $L.rec.ms 1e12 | grep -E 'RECORD|records'; echo "rc=${PIPESTATUS[0]}"
echo "== bad arguments (expect rc 1)"
./gilbreath 100 1024 4096 2 64 > /dev/null 2>&1; echo "W=2 rc=$?"
./gilbreath 100 4096 4096 10 64 > /dev/null 2>&1; echo "K=4096 rc=$?"
./gilbreath 105 1024 4096 10 64 > /dev/null 2>&1; echo "X%W rc=$?"
BENCH_LO=10 ./gilbreath 100 1024 4096 10 64 $L.x > /dev/null 2>&1; echo "BENCH_LO+log rc=$?"
SEG_LO=0 SEG_HI=1 ./gilbreath 1000 1024 4096 100 64 > /dev/null 2>&1; echo "SEG_LO/SEG_HI without log rc=$?"
echo "== done"
