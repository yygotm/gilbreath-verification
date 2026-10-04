#!/bin/bash
# Progress of a large-range run (run on the host; needs python3): segment counts and milestones.
cd "$(dirname "$0")/.." || exit 1
L=${1:-prod/large.log}
[ -f "$L" ] || { echo "no log yet"; exit 0; }
n=$(grep -c '^DONE' "$L"); f=$(grep -c '^FAIL' "$L")
echo "DONE segments: $n   FAIL lines: $f"
echo "last DONE: $(grep '^DONE' "$L" | tail -n 1 | cut -d'|' -f1)"
python3 milestones.py "$L" 1e13 1e14 1e15 2e15 5e15 1e16 2>&1 | grep -v 'segments missing'
