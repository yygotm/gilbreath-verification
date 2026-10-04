#!/bin/bash
set -u
cd /w || exit 1
[ ! -e prod/home_1e13.log ] || { echo 'Existing log; refusing to overwrite timing run'; exit 1; }
# Use the supplied default sieve size and the 16 threads used in the passed home tests.
kib=32
th=16
export SIEVE_KIB=$kib OMP_NUM_THREADS=$th
printf 'CPU kib=%s threads=%s K=1535 B=8192 W=100000000000 T=128\n' "$kib" "$th" > prod/home_1e13_settings.out
TIMEFORMAT='elapsed_seconds=%3R'
{ time prod/gilbreath_prod 10000000000000 1535 8192 100000000000 128 prod/home_1e13.log > prod/home_1e13.out 2> prod/home_1e13.err; } 2> prod/home_1e13.timing
rc=$?
printf '%s\n' "$rc" > prod/home_1e13.exit
[ "$rc" -eq 0 ] || exit "$rc"
python3 milestones.py prod/home_1e13.log 1e11 1e12 1e13 > prod/home_1e13_milestones.out 2>&1 || exit 1
python3 verify_home_1e13.py > prod/home_1e13_verified.out 2>&1 || exit 1
cat prod/home_1e13_settings.out prod/home_1e13_verified.out