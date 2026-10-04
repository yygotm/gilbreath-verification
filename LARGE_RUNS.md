# Running larger ranges

The release reproduces the computation up to 10^13. The same program can be extended to larger
bounds (it was prepared for 10^16). This guide is for anyone with more computing power who wants
to push the verified range further. Runs can be stopped and resumed at any time, and the work can
be split across machines.

## How the computation is organized

`gilbreath X K B W T logfile` processes the primes below `X` in segments of width `W`.
Each completed segment appends one `DONE` line to `logfile`; a rerun skips segments already in the
log. G for any milestone `x` (a multiple of `W`) is obtained by combining the `DONE` lines of all
segments below `x` (`milestones.py`).

Use the same `K B W T` (and the same `gilbreath.c`) for the whole run. They are part of every
log line's tag; changing them means starting over.

| parameter | value | meaning |
|---|---|---|
| K | 1535 | largest row tracked. If G would exceed K, the program stops with exit code 5 |
| B | 8192 | positions per block |
| W | 10^11 | segment width (10^5 segments up to 10^16) |
| T | 128 | minimal run of {0, 2} entries used to split exceptional runs |

## Build and start (Docker)

```bash
docker build -t gilb-env .
docker run --rm -v "$PWD:/w" gilb-env bash -c "sed 's/\r$//' /w/run_tests.sh | bash" > tests_local.out 2>&1
docker run --rm -v "$PWD:/w" gilb-env bash -c "sed 's/\r$//' /w/prod/build.sh | bash"
docker run -d --name gilb-large -v "$PWD:/w" gilb-env bash -c "sed 's/\r$//' /w/prod/run_large.sh | bash"
```

Check `tests_local.out` against `tests.out` before starting. Do not use `--restart`: a failure
should stop the run, not repeat it.

- Stop: `docker stop -t 600 gilb-large` (finishes the current segment; exit code 9)
- Resume: `docker start gilb-large`
- Progress and milestones: `bash prod/status.sh`
- Upper limit: add `-e X=2000000000000000` to `docker run` (the default is 10^16)
- Tuning (does not affect results): `-e SIEVE_KIB=32`, `-e OMP_NUM_THREADS=8`

Near 10^15, one segment took about 15 s on an AMD Ryzen 7 5700X (16 threads) and about 20 s on an
Intel Core i7-1355U (12 threads). The full range to 10^16 is therefore roughly 2–3 weeks of
continuous computation on one such machine.

## Exit codes

| code | meaning |
|---|---|
| 0 | all segments done (ALLDONE) |
| 4 | measurement-only run (BENCH_LO, SIEVE_ONLY, RECHECK); not a verification |
| 5 | K too small |
| 7 | a FAIL line or a conflicting DONE line is present in the log |
| 8 | the log is locked by another process |
| 9 | stopped by a signal; resume to continue |
| 2, 3, 6 | other failures; see `prod/large_run.err` and the `reason=` field of FAIL lines |

A FAIL line with `reason=counterexample_candidate` is either a hardware error or a counterexample.
Recompute that segment on a different machine before drawing any conclusion.

## Splitting across machines

Run the same command with `SEG_LO` / `SEG_HI` (segment index range, e.g. `-e SEG_LO=0 -e SEG_HI=49999`)
on each machine, each with its own log file. Concatenate the logs (`cat a.log b.log > large.log`)
and rerun with the full range: if every segment is present, the program reports ALLDONE.
Computing a few segments on both machines lets the program detect disagreements (CONFLICT, exit 7).

## Spot checks

To detect hardware errors, recompute a few random completed segments into a separate log and
compare the `DONE` lines:

```bash
RECHECK=123,4567,8901 prod/gilbreath_prod 10000000000000000 1535 8192 100000000000 128 prod/recheck.log   # exit 4 is normal
for i in 123 4567 8901; do
  diff <(grep "^DONE $i " prod/large.log | sed -E 's/ rows=.* reason=[a-z_]* / /; s/ sieve=[a-z]* / /') \
       <(grep "^DONE $i " prod/recheck.log | sed -E 's/ rows=.* reason=[a-z_]* / /; s/ sieve=[a-z]* / /') && echo "seg $i SAME"
done
```

## Reference values

Milestones that must match known results before trusting new ranges:

| x | π(x) (OEIS A006880) | G(π(x)) |
|---|---:|---:|
| 10^13 | 346,065,536,839 | 635 (Odlyzko 1993) |
| 10^14 | 3,204,941,750,802 | 693 (Plouffe, arXiv:2510.06688) |
| 10^15 | 29,844,570,422,669 | 800 (Colonna) |
| 10^16 | 279,238,341,033,925 | unknown |

`milestones.py` also checks the record prime gaps (OEIS A002386/A005250) in every completed segment.

If you extend the range with this code, a citation of the Zenodo record
(https://doi.org/10.5281/zenodo.23160565) is appreciated.
