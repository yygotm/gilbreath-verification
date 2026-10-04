// Gilbreath の予想の素朴な参照実装（gilbreath.c と共有するコードなし。素数は自前のエラトステネスのふるい）。
// d_0(n) = p_n、d_k(n) = |d_{k-1}(n+1) - d_{k-1}(n)|。Odlyzko (1993) の定義に従い、
// G(N) = 最小の k で、d_j(1) = 1（1 <= j <= k）かつ d_k(n) ∈ {0, 2}（2 <= n <= N）を満たすもの。
// N = π(X)（X 未満の素数の個数）。行 k の位置 n は p_n..p_{n+k} を使うので、X より先の素数も KMAX 個使う。
// 使い方: brute X [KMAX]   出力: X, π(X), G(π(X))
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: brute X [KMAX]\n"); return 1; }
    uint64_t X = strtoull(argv[1], NULL, 10);
    long KMAX = argc > 2 ? atol(argv[2]) : 2000;
    // X 以降に KMAX + 2 個の素数が入るまでふるいの上限を広げる
    uint64_t Y = X + 64 * (uint64_t)KMAX + 1000;
    for (;;) {
        uint8_t *c = calloc(Y + 1, 1);
        if (!c) { fprintf(stderr, "out of memory\n"); return 3; }
        for (uint64_t i = 2; i * i <= Y; i++) if (!c[i]) for (uint64_t j = i * i; j <= Y; j += i) c[j] = 1;
        uint64_t N = 0, tot = 0;
        for (uint64_t i = 2; i <= Y; i++) if (!c[i]) { tot++; if (i < X) N++; }
        if (tot < N + (uint64_t)KMAX + 2) { free(c); Y = Y * 2; continue; }
        uint64_t L = N + KMAX + 1;                              // 使う素数の個数
        int64_t *d = malloc(sizeof(int64_t) * L);
        uint64_t k = 0;
        for (uint64_t i = 2; i <= Y && k < L; i++) if (!c[i]) d[k++] = (int64_t)i;
        free(c);
        long G = -1;
        for (long r = 1; r <= KMAX; r++) {
            uint64_t len = L - r;                               // 行 r の長さ
            for (uint64_t i = 0; i < len; i++) { int64_t v = d[i + 1] - d[i]; d[i] = v < 0 ? -v : v; }
            if (d[0] != 1) { printf("COUNTEREXAMPLE row=%ld d(1)=%lld\n", r, (long long)d[0]); return 2; }
            int clean = 1;
            for (uint64_t i = 1; i < N && i < len; i++) if (d[i] != 0 && d[i] != 2) { clean = 0; break; }
            if (clean) { G = r; break; }
        }
        printf("X=%llu pi=%llu G=%ld\n", (unsigned long long)X, (unsigned long long)N, G);
        free(d);
        return G < 0 ? 4 : 0;
    }
}
