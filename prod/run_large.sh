#!/bin/bash
# Large-range run inside the container (/w = repository root). Default: all primes below 10^16.
# Stop with: docker stop -t 600 <name>   (SIGTERM; the segment in progress is finished, exit code 9)
# Resume with: docker start <name>       (completed segments in the log are skipped)
# Parameters: K=1535 (maximum row; exit code 5 if too small), B=8192 (block size),
#             W=10^11 (segment width), T=128. Do not change K, B, W or T once a log exists.
# Optional: X (upper limit), SIEVE_KIB, OMP_NUM_THREADS, SEG_LO/SEG_HI (segment index range).
cd /w || exit 1
X=${X:-10000000000000000}
P=prod/gilbreath_prod
L=prod/large.log
exec $P "$X" 1535 8192 100000000000 128 $L > prod/large_run.out 2> prod/large_run.err
