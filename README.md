# A Reproducible CPU Verification of Gilbreath's Conjecture up to 10^13

Hirotaka Shimizu

CPU source code and execution evidence for a finite verification of the Proth–Gilbreath conjecture, using Odlyzko's definition of G(N).

Research, implementation, optimization, and the original computation were performed on an Intel Core i7-1355U system with 12 threads. The recorded original result, subsequently reproduced on an AMD Ryzen 7 5700X, is:

| Position cutoff | Prime count | G |
|---|---:|---:|
| 10^11 | 4,118,054,813 | 417 |
| 10^12 | 37,607,912,018 | 481 |
| 10^13 | 346,065,536,839 | 635 |

G = 635 agrees with the established result. The contribution is an inspectable, reproducible implementation with source code, tests, complete segment logs, reproduction instructions, and file checksums.

## Inspect the evidence

With Python 3, run this from the extracted package root:

```bash
python3 verify_bundle.py
```

This checks the saved evidence without starting a new prime-generation or difference computation. It verifies the accompanying file hashes, test transcript, complete segment coverage, combined row masks, prime counts, G, milestones, and recorded exit statuses.

See [REPRODUCE.md](REPRODUCE.md) for a fresh build, prerequisite tests, and a complete 10^13 rerun in a separate working copy. See [PUBLICATION_DRAFT.md](PUBLICATION_DRAFT.md) for the mathematical definitions, algorithm, prior results, and validation scope.

## Subsequent reproduction environment

AMD Ryzen 7 5700X, 16 threads, a Linux container on WSL2, GCC 12.2.0 targeting znver3, primesieve 11.0, and a 32 KiB sieve. Parameters: K=1535, B=8192, W=10^11, T=128, SELFCHECK=0.

Bash reported 1135.578 seconds, approximately 18 minutes 56 seconds. The program's internal timer reported 1135.952 seconds. Both records are retained. These are timings for the subsequent AMD reproduction check, not for the original Intel research. They exclude setup, compilation, prerequisite tests, and post-run checking.

CPU builds require x86-64 AVX2 and primesieve 11.x. The source accesses primesieve iterator internals. The Dockerfile's mutable tag and apt inputs do not guarantee byte-identical builds.

## Validation scope

The final released CPU implementation was rerun on the AMD system and its complete log was checked against the recorded prime counts and G values. The included 10^13 segment log is from this reproduction check; the original Intel 10^13 log is not included and the two original segment logs were not compared line by line. Small-input tests also compared a simple implementation and the optimized/reference paths. This is not an independent reimplementation of the entire 10^13 computation, a proof of the conjecture for all rows, or a new numerical record. GPU hardware execution is outside this release.

The original verified source and scripts are preserved byte-for-byte, including their original comments. The English guide explains their use. Checkpoint source identifiers and original logs remain aligned.

## Human–AI collaboration

I carried out this work in collaboration with AI assistants. Claude assisted with the research, implementation, optimization, and implementation reviews on the Intel system. OpenAI Codex assisted with the subsequent reproduction checks and preparation of the release materials. I directed the work and retain responsibility for the released materials and claims. AI-assisted reviews and reproduction checks are not presented as an independent mathematical audit.

## References

- A. M. Odlyzko, “Iterated absolute values of differences of consecutive primes,” Mathematics of Computation 61 (1993), 373–380. [Author's bibliography](https://www-users.cse.umn.edu/~odlyzko/doc/cnt.html).
- [Simon Plouffe, arXiv:2510.06688](https://arxiv.org/abs/2510.06688).
- [Jean-François Colonna, Proth-Gilbreath computations](https://www.lactamme.polytechnique.fr/descripteurs/GilbreathConjecture.01.Ang.html).
- [OEIS A006880](https://oeis.org/A006880/b006880.txt), [A002386](https://oeis.org/A002386/b002386.txt), and [A005250](https://oeis.org/A005250/b005250.txt).

## Release and citation

- Archive: Zenodo, DOI [10.5281/zenodo.23160565](https://doi.org/10.5281/zenodo.23160565)
- Source and evidence: <https://github.com/yygotm/gilbreath-verification>

Please cite this work using the author, title, and DOI above (see `CITATION.cff`).

## License

MIT License. See `LICENSE`.
