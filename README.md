# Gilbreath's conjecture: reproducible CPU verification up to 10^13

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.23160565.svg)](https://doi.org/10.5281/zenodo.23160565)

## Claim

Let `p_n` be the n-th prime, `d_0(n) = p_n` and `d_k(n) = |d_{k-1}(n+1) - d_{k-1}(n)|`.
Gilbreath's conjecture (Proth 1878, Gilbreath 1958) states that `d_k(1) = 1` for every `k >= 1`.

With `G(N)` as defined by Odlyzko (1993) — the smallest `k` such that `d_j(1) = 1` for `1 <= j <= k`
and `d_k(n)` is 0 or 2 for `2 <= n <= N` — this repository computes:

| x | π(x) | G(π(x)) |
|---|---:|---:|
| 10^11 | 4,118,054,813 | 417 |
| 10^12 | 37,607,912,018 | 481 |
| 10^13 | 346,065,536,839 | 635 |

This reproduces Odlyzko's 1993 result. It is not a new bound: the reported frontier is
10^14 (Plouffe, arXiv:2510.06688) and 1.5 × 10^15 (Colonna, 2026). As far as we know, none of
these computations published their code or logs; this repository does.

## Method

Primes are generated with primesieve 11.x. Difference rows are computed with 8-bit SIMD; entries
other than 0 and 2 stay in triangular runs below large prime gaps, so only those runs are computed
and the rest advances by 1-bit xor. The range is split into segments of width 10^11; each segment
writes one `DONE` line, and `milestones.py` combines them into π(x) and G(π(x)).

## Validation

- G agrees with Odlyzko's values for 10^2 to 10^9, 10^11 and 10^12.
- π(10^13) agrees with OEIS A006880.
- The 21 record prime gaps below 10^12 (OEIS A002386/A005250) match the per-segment maximal gaps.
- A self-check mode compares the optimized path with a reference path; `brute.c` is an
  independent brute-force implementation for small inputs.
- Checkpoint tests: resume, extension, truncated lines, conflicts, locking, signals, split runs.

## Files

| file | content |
|---|---|
| `gilbreath.c` | the verifier |
| `brute.c` | brute-force reference for small inputs |
| `milestones.py` | π(x), G(π(x)) and record-gap checks from a log |
| `run_tests.sh`, `tests.out`, `tests_home.out` | test suite and its transcripts |
| `prod/home_1e13.*` | the 10^13 run: all 100 segment lines, output, timing, exit status |
| `verify_bundle.py` | checks the saved evidence without recomputing |
| `REPRODUCE.md` | fresh build and full 10^13 rerun |
| `LARGE_RUNS.md` | running larger ranges (prepared for 10^16; stoppable, resumable, splittable) |
| `PUBLICATION_DRAFT.md` | technical description |
| `Dockerfile`, `SHA256SUMS` | environment and checksums |

Check the saved evidence with `python3 verify_bundle.py`.

## Environment

Developed and first run on an Intel Core i7-1355U (12 threads). The included segment logs are from
a full rerun on an AMD Ryzen 7 5700X (16 threads, Docker on WSL2, GCC 12.2.0, primesieve 11.0):
1,136 s for all primes below 10^13. Requires x86-64 with AVX2 and primesieve 11.x.

## References

- A. M. Odlyzko, "Iterated absolute values of differences of consecutive primes,"
  Mathematics of Computation 61 (1993), 373–380.
- S. Plouffe, [arXiv:2510.06688](https://arxiv.org/abs/2510.06688).
- J.-F. Colonna, [Proth-Gilbreath conjecture computations](https://www.lactamme.polytechnique.fr/descripteurs/GilbreathConjecture.01.Ang.html).
- OEIS [A006880](https://oeis.org/A006880), [A002386](https://oeis.org/A002386), [A005250](https://oeis.org/A005250).

## Citation and license

Zenodo: https://doi.org/10.5281/zenodo.23160565 (see `CITATION.cff`). MIT License.

AI-assisted (Claude, Anthropic; OpenAI Codex).
