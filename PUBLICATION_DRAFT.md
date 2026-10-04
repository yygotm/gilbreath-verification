# A Reproducible CPU Verification of Gilbreath's Conjecture up to 10^13

Hirotaka Shimizu

## Abstract

We present a CPU implementation for a finite verification of the Proth–Gilbreath conjecture. Research, implementation, optimization, and the original computation were performed on an Intel Core i7-1355U system using 12 threads. The original computation recorded π(10^13) = 346,065,536,839 and G(π(10^13)) = 635. A subsequent complete rerun of the final CPU implementation on an AMD Ryzen 7 5700X reproduced these values. The accompanying source, tests, rerun logs, and checking procedures make the released implementation inspectable and reproducible. The value G = 635 agrees with the established result; this work claims neither a new numerical record nor a proof for all rows. The work was conducted in collaboration with AI assistants, whose roles are disclosed below.

## Background and verification target

The conjecture is also known as the Proth–Gilbreath conjecture. [Colonna's historical account](https://www.lactamme.polytechnique.fr/descripteurs/GilbreathConjecture.01.Ang.html) attributes its earlier publication to François Proth in 1878 and its later formulation to Norman L. Gilbreath in 1958. The computational quantity G(N) used here follows Odlyzko's 1993 work.

Let p_n denote the nth prime, and define d_0(n) = p_n and d_k(n) = |d_{k-1}(n+1) − d_{k-1}(n)|. Gilbreath's conjecture states that d_k(1) = 1 for every k ≥ 1.

G(N) is the smallest k such that d_j(1) = 1 for 1 ≤ j ≤ k and d_k(n) belongs to {0, 2} for 2 ≤ n ≤ N. Here N is the number of primes below 10^13. Because d_k(n) depends on p_n through p_{n+k}, additional primes beyond the position cutoff are generated. “Up to 10^13” refers to the prime values identifying the checked positions, rather than the largest prime generated.

Once G(N) = g is established, |1 − 0| = |1 − 2| = 1 and closure of {0, 2} under absolute differences imply that the leading entries remain 1 through a finite range of subsequent rows. This does not establish the conjecture for every k.

## Implementation and original research

The implementation was developed and optimized on an Intel Core i7-1355U with 12 threads. Prime generation uses primesieve 11.x. Numerical intervals are divided into blocks of positions. Each block records a bit mask of rows satisfying the required condition; the first row in their intersection determines G. The condition for a fixed position range need not be monotone in the row number, so the last disappearance of an exceptional entry alone does not determine G.

The first block and other blocks requiring the reference path use straightforward 16-bit calculations. Eligible blocks divide even values by two and use 8-bit AVX2 operations. Runs containing entries outside {0, 2} are represented explicitly, while other regions advance using XOR on a one-bit representation. Segment records include a source identifier and parameters. The implementation supports checkpoint resumption, conflict detection, and explicit failure reasons.

The research notes record a benchmark near 10^15 over a numerical interval of width 10^11: the median of three interleaved runs decreased from 26.8 seconds for the earlier implementation to 20.3 seconds for the optimized implementation with B=8192 and T=128. Prime generation alone took approximately 15.0–15.7 seconds. These measurements belong to the Intel research and optimization phase; they are not timings for a full 10^13 rerun on the AMD system.

## Original result and evidence provenance

| Position cutoff X | Prime count π(X) | G(π(X)) |
|---|---:|---:|
| 10^11 | 4,118,054,813 | 417 |
| 10^12 | 37,607,912,018 | 481 |
| 10^13 | 346,065,536,839 | 635 |

The contemporaneous research notes record completion of the original Intel computation through 10^13 using the v5 implementation with K=1024, B=8192, W=10^11, and T=128. Later implementation reviews led to additional guards and the final released source; the reproduction run used K=1535. These are different stages of the work.

The original Intel 10^13 segment log and binary are not included in this release. The original result above is documented in the research notes. The included complete 10^13 segment log is from the subsequent AMD reproduction run of the final released CPU source. Agreement was checked with the recorded prime counts and G values; the two machines' original segment logs were not compared line by line.

## Subsequent reproduction check

The final implementation was rerun on an AMD Ryzen 7 5700X with 16 logical threads, in a Linux container on WSL2. The environment used GCC 12.2.0 with -O3 -march=native -fopenmp targeting znver3, and libprimesieve-dev 11.0+ds-2. Reproduction parameters were a 32 KiB sieve, K=1535, B=8192, W=10^11, T=128, and SELFCHECK=0.

The reproduction log contains 100 distinct, contiguous segments with no missing or duplicate segments, FAIL records, or conflicts. The program and checking runner exited with status 0, and program standard error was empty. Intersecting all segment masks reproduces G=635. The prime-count milestones agree with [OEIS A006880](https://oeis.org/A006880/b006880.txt).

The checker uses 26 record prime-gap reference entries from [OEIS A002386](https://oeis.org/A002386/b002386.txt) and [A005250](https://oeis.org/A005250/b005250.txt). It compares segment maxima with the largest record in each segment and checks for larger-than-expected gaps. When multiple records occur in one segment, they are represented by that segment's largest record. The count of 26 is a count of covered reference entries, not 26 individually and independently recomputed gaps. No mismatches were found.

Bash recorded 1135.578 seconds for this AMD reproduction run, while the internal timer recorded 1135.952 seconds. The cause of the 0.374-second difference has not been established. Both original records are retained. These timings exclude setup, compilation, prerequisite tests, and post-run checks, and are not the execution time of the original Intel research.

Prerequisite tests compared small inputs with a simple implementation and optimized paths with reference paths through SELFCHECK. They also exercised resumption, extension, boundary cases, conflicting logs, locking, insufficient K, split-and-merge execution, SIGTERM, and corrupted records. The AMD test transcript matched all 100 lines of the included original reference transcript after excluding elapsed times and the valid SIGTERM completion count.

This is a reproduction check using the same implementation, not an independently developed verifier for the entire 10^13 range or a comprehensive mathematical audit. GPU hardware execution is outside the released result.

## Human–AI collaboration

I carried out this work in collaboration with AI assistants. Claude assisted with the research, implementation, optimization, and implementation reviews on the Intel system. OpenAI Codex assisted with the subsequent reproduction checks and preparation of the release materials. I directed the work and retain responsibility for the released materials and claims. AI-assisted reviews and reproduction checks are not presented as an independent mathematical audit.

## Prior results and accompanying materials

Odlyzko's 1993 work reported verification up to 10^13. The established G=635 is also stated on [Colonna's author page](https://www.lactamme.polytechnique.fr/descripteurs/GilbreathConjecture.01.Ang.html). Plouffe reports verification up to 10^14 in [arXiv:2510.06688](https://arxiv.org/abs/2510.06688), and Colonna reports verification up to 1.5 × 10^15. These are cited as the respective authors' reported results.

See REPRODUCE.md for the accompanying files and reproduction instructions, and research_provenance.json for the distinction between original research and reproduction evidence. File hashes are listed in SHA256SUMS.

Release DOI: [10.5281/zenodo.23160565](https://doi.org/10.5281/zenodo.23160565).

Reference: A. M. Odlyzko, “Iterated absolute values of differences of consecutive primes,” Mathematics of Computation 61 (1993), 373–380. [Author's bibliography](https://www-users.cse.umn.edu/~odlyzko/doc/cnt.html).
