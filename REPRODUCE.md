# Reproduction Guide

The original research and optimization were performed on an Intel Core i7-1355U system with 12 threads. This release contains the final CPU source and saved evidence from a subsequent reproduction run on an AMD Ryzen 7 5700X. The included 10^13 segment log is from that reproduction run, not the original Intel computation. The work was carried out with AI assistance; see PUBLICATION_DRAFT.md for the disclosed roles. Preserve the original evidence and run new computations in a separate working copy. Use Docker Desktop's Linux engine on Windows, or Docker on Linux. CPU builds target x86-64 systems with AVX2 and require primesieve 11.x. See environment_public.json for the reproduction-run environment and research_provenance.json for the original-research/reproduction distinction. The Dockerfile uses a mutable image tag and apt repositories; it does not guarantee byte-for-byte reproducible builds.

## 1. Check the saved evidence without recomputation

Run the following in the release directory using Python 3 in normal mode. It does not start the computational verifier.

```bash
python3 verify_bundle.py
```

The checker validates SHA256SUMS, the source identifier, all 100 saved test-transcript lines, all 100 segment records, π, G, milestones, exit statuses, empty standard error, and both recorded timings. Its final line begins with BUNDLE VERIFIED. Checksums establish consistency of the accompanying files; they do not prove mathematical correctness.

## 2. Rebuild and rerun the prerequisite tests in a fresh copy

From the release root, select a destination that does not exist. If gilbreath-reproduction already exists, use another name. The following examples use Bash, including Bash in WSL on Windows.

```bash
test ! -e ../gilbreath-reproduction || exit 1
mkdir ../gilbreath-reproduction
cp -a . ../gilbreath-reproduction/
cd ../gilbreath-reproduction
docker build -t gilb-reproduction-env .
docker run --rm --network none -e OMP_NUM_THREADS=16 -v "$PWD:/w" -w /w gilb-reproduction-env bash -c '
set -e
bash run_tests.sh > tests_reproduced.out 2> tests_reproduced.err
test ! -s tests_reproduced.err
bash prod/build.sh
'
```

When invoking Docker directly from PowerShell, use `"${PWD}:/w"` for its volume argument. The copy and shell steps above are Bash commands. If an explicit Docker endpoint is needed on Windows, add `-H npipe:////./pipe/dockerDesktopLinuxEngine` to each docker command.

run_tests.sh deliberately exercises expected failures, so exit status 0 alone is insufficient to establish success. Compare every line of the new transcript as follows. The checker also verifies that the number of segments completed before SIGTERM is at least 0 and less than 100.

```bash
docker run --rm --network none -v "$PWD:/w:ro" -w /w gilb-reproduction-env python3 verify_bundle.py --tests tests_reproduced.out
```

Success is reported as REPRODUCED TEST TRANSCRIPT VERIFIED. The prerequisite suite includes a run up to 10^12. During release preparation, only the saved evidence was checked; this suite was not rerun.

## 3. Recompute 10^13 into a new log

Execute this step if you choose to perform a new full run. Do not append to the saved home_1e13.log. In the rebuilt working copy, run:

```bash
docker run --rm --network none -e OMP_NUM_THREADS=16 -e SIEVE_KIB=32 -v "$PWD:/w" -w /w gilb-reproduction-env bash -c '
set -euo pipefail
unset SELFCHECK BENCH_LO SIEVE_ONLY RECHECK SEG_LO SEG_HI GPU
test ! -e prod/reproduced_1e13.log
TIMEFORMAT="elapsed_seconds=%3R"
set +e
{ time prod/gilbreath_prod 10000000000000 1535 8192 100000000000 128 prod/reproduced_1e13.log > prod/reproduced_1e13.out 2> prod/reproduced_1e13.err; } 2> prod/reproduced_1e13.timing
rc=$?
set -e
printf "%s\n" "$rc" > prod/reproduced_1e13.exit
test "$rc" -eq 0
test ! -s prod/reproduced_1e13.err
python3 milestones.py prod/reproduced_1e13.log 1e11 1e12 1e13 > prod/reproduced_1e13_milestones.out
python3 verify_bundle.py --run-prefix prod/reproduced_1e13
'
```

Success requires 100 contiguous segments, the expected fixed tag, reason=none, sieve=cpu, no FAIL records or conflicts, π=346065536839, G=635, milestones 417/481/635, and no record-gap mismatches. Runtime depends on the environment and is not required to equal the saved 1135.578 seconds. The checker intersects the clean masks across all segments; individual segment G values are insufficient.

To resume an interrupted run, use the same build, parameters, and log. Resumption timings measure only unfinished work and must be distinguished from a complete fresh run. The included run_home_1e13.sh is the original runner that produced the saved evidence; it refuses to run while that evidence log already exists.

## Evidence and release scope

The release includes the CPU source, brute.c, test scripts, Dockerfile, build script, milestones.py, original checking scripts, original test output, and the 10^13 log, output, exit statuses, timings, and parameters. It excludes compiled binaries, the GPU directory, the incomplete parameter sweep, private handoffs, and review conversations. environment_public.json extracts the necessary environment facts; it is not a verbatim copy of the original environment transcript.

The release is licensed under the MIT License (see LICENSE). Archive DOI: 10.5281/zenodo.23160565.
