// Gilbreath の予想の検証（v2: 例外を連続区間（run）で扱う）。
// d_0(n) = p_n、d_k(n) = |d_{k-1}(n+1) - d_{k-1}(n)|。予想: すべての k >= 1 で d_k(1) = 1。
// Odlyzko (1993) の G(N): 最小の k で、d_j(1) = 1（1 <= j <= k）かつ d_k(n) ∈ {0, 2}（2 <= n <= N）。
// G(N) <= K が求まれば、0 と 2 の差は 0 か 2、|1 - 0| = |1 - 2| = 1 なので、d_j(1) = 1 が j < N + G(N) で成り立つ。
//
// 方法: 位置 n（X 未満の素数）を幅 W の区間に分け（OpenMP で並列）、区間をさらに B 個ずつの塊にする。
// 塊 [a, b) の行 k は p_a..p_{b-1+k} だけで決まるので、窓 [a, b + K) の素数で K 行まで計算できる。
// 塊ごとに「[a, b) が {0, 2} だけ（位置 1 は 1）になる行」の集合をビット列で求め、全塊の AND の最初の行を G とする。
//   （位置 N の行 k+1 は位置 N+1 も使うので、[2, N] の上での「汚れなし」は行について単調ではない。最後の塊で効く。）
// 窓全体が {0, 2} だけになった行より先は、窓の中で {0, 2} のままなので、そこで打ち切り、残りの行を汚れなしとする。
// K 行までに全塊で汚れなしの行が無ければ失敗（K を増やす）。
//
// 行の計算（block_runs8）: 行の値のうち {0, 2} 以外（例外）は、大きな素数の差の下に三角形に固まって現れる。
//   窓を「例外を含む連続区間（run）」と「{0, 2} だけの領域」に分ける。{0, 2} だけの領域は 1 ビット（2 なら 1）で持ち、
//   両隣が {0, 2} なら |a - b| = 2 (a/2 xor b/2) なので、1 語 64 個ずつ xor で次の行に進む。
//   run の中だけ値（半分にして 8 ビット）を AVX2 で |a - b| と計算する。run は左に 1 つ広がり（左隣の値は例外の影響を受ける）、
//   右端は窓の中で閉じる。計算のあと run の両端の {0, 2} を刈り、中に {0, 2} が T 個以上続く所で分割する。
//   run の端は必ず例外なので、塊の汚れの判定は run の端の位置で済む（塊をまたぐ run だけ中を見る）。
//   最初は窓全体を密に計算し（段 A）、{0, 2} が 32 個以上続く所が現れたら run に移る（段 B）。分割が進むにつれて計算量が減る。
//   先頭の塊（素数 2 を含む。差 1）と、窓に 510 を超える差がある塊（10^16 未満では約 0.4%、推測）は参照経路で処理する。
// 参照経路（block_ref）: 16 ビットの値で全行を素朴に計算する。SELFCHECK=1 で両者の行の集合を比べる（不一致は失敗）。
//
// 使い方: gilbreath X K B W T [logfile]
//   X 未満の素数を位置とする（X は W の倍数）。K = 最大の行数、B = 塊の位置数、W = 区間の幅、T = run を分割する {0, 2} の最小の連続数（0 なら分割しない）。
//   logfile を与えると区間ごとに DONE 行を追記し（fsync）、再実行では済んだ区間を飛ばす。全区間が済んだら ALLDONE 行を書く。
// 環境変数: SELFCHECK=1（参照経路と比較）、BENCH_LO=<n>（計測用: [BENCH_LO, X) の区間だけ。検証にならない）、
//           SIEVE_ONLY=1（計測用: 素数を取り出すだけ）、SIEVE_KIB=<n>（primesieve の篩の大きさ。既定 32）、RECHECK=<i,j,...>（区間の再計算）、
//           SEG_LO/SEG_HI=<n>（この実行が受け持つ区間番号の範囲。複数の計算機で分担するとき。logfile が必須）、
//           GPU=1（-DGPU_SRC でビルドしたとき、素数の出どころを GPU の篩 gpu/gpusrc.cpp にする。区間の先頭が 2^33 以上の区間だけ。DONE 行の sieve= に残る）
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <signal.h>
#include <primesieve.h>
#include <omp.h>
#include <immintrin.h>
#ifdef GPU_SRC
#include "gpu/gpusrc.h"                                           // 素数の出どころを GPU の篩にする（GPU=1 のとき。区間の先頭が 2^33 以上の区間だけ）
#endif

// segment() は primesieve の iterator の内部（it.i, it.size, it.primes, primesieve_generate_next_primes）を直接使う。
// 11.0 で確かめた（SELFCHECK では区間の素数の個数を primesieve_count_primes と照合する）。別の版では中身を確かめてから広げる。
#if !defined(PRIMESIEVE_VERSION_MAJOR) || PRIMESIEVE_VERSION_MAJOR != 11
#error "checked only with primesieve 11.x (segment() relies on the iterator internals); verify and extend this guard for other versions"
#endif
#ifndef SRC_HASH
#error "build with -DSRC_HASH=... (md5 of the source; keeps logs of other builds from being reused)"
#endif
// 失敗の種類（ビットで OR 集計する。反例候補が他の失敗に隠れないように）
#define F_CE 1                                                    // d_k(1) != 1（反例候補）
#define F_SC 2                                                    // SELFCHECK 不一致
#define F_K 4                                                     // K 行では足りない（区間内に全塊が汚れなしの行が無い）
#define F_GAP 8                                                   // 素数の差が 16 ビットに収まらない（GPU 経路では差が 0 か奇数）
#define F_NOP 16                                                  // 幅 4096 以上の区間に素数が 1 つも無い（primesieve の失敗の疑い）
#define KW 64                                                     // 行の集合のビット列の語数（K <= 64*KW - 1）
static long K, B, T;
static uint64_t X, W;
static int SELFCHECK, SIEVE_ONLY, USE_GPU;                       // USE_GPU: GPU_SRC でビルドし GPU=1 のとき                                 // SIEVE_ONLY=1: 計測用。素数を取り出すだけで行を計算しない
static volatile sig_atomic_t stop_req;                             // SIGTERM/SIGINT: 進行中の区間を終えてから止める（docker stop -t <秒> で待つ）
static void on_signal(int sig) { (void)sig; stop_req = 1; }

typedef struct { uint64_t npos, rows, cells, refblocks, arows, brows, bruns, bgroups; uint64_t clean[KW]; int fail; long ce_row; unsigned ce_val; unsigned maxgap; uint64_t maxgap_p; int gpu; } seg_t;   // maxgap@maxgap_p: 区間の位置の素数 p と次の素数の差の最大（最初に現れたもの。公開表 A002386/A005250 との照合用）、gpu: 素数の出どころが GPU   // ce_row/ce_val: 反例候補の行と位置 1 の値   // arows/brows: 段 A/B の行数、bruns/bgroups: 段 B で処理した run と 64 セル組の数   // fail は F_* の OR。cells = 計算したセルの数、refblocks = 参照経路で処理した塊の数

static void *xmalloc(size_t n) { void *p = malloc(n); if (!p) { fprintf(stderr, "out of memory (%zu bytes)\n", n); exit(3); } return p; }
static inline void setbit(uint64_t *m, long k) { m[k >> 6] |= 1ULL << (k & 63); }
static inline int getbit(const uint64_t *b, long j) { return (int)(b[j >> 6] >> (j & 63) & 1); }
static long first_clean(const uint64_t *c) { for (long k = 1; k <= K; k++) if (c[k >> 6] >> (k & 63) & 1) return k; return -1; }

// ---- 参照経路: 16 ビットの値で全行を素朴に計算する（速さより分かりやすさ。SELFCHECK と小さな範囲の試験用） ----
// w[0..L) は行 1（素数の差）。nb 個の位置が塊。first なら w[0] は位置 1。m に汚れなしの行のビットを立てる。戻り値 -k = 行 k で位置 1 が 1 でない（*ce_val にその値）
static int block_ref(uint16_t *w, long L, long nb, int first, uint64_t *m, unsigned *ce_val) {
    memset(m, 0, sizeof(uint64_t) * KW);
    for (long k = 1; k <= K; k++) {
        long Lk = L - (k - 1);                                         // 行 k の長さ
        if (k > 1) for (long i = 0; i < Lk; i++) { uint16_t x = w[i + 1], y = w[i]; w[i] = x > y ? x - y : y - x; }
        if (first && w[0] != 1) { *ce_val = w[0]; return (int)-k; }
        long s = first ? 1 : 0, r = nb < Lk ? nb : Lk; int din = 0, dout = 0;
        for (long i = s; i < r; i++) if (w[i] & 0xFFFD) { din = 1; break; }   // {0, 2} 以外
        for (long i = r; i < Lk; i++) if (w[i] & 0xFFFD) { dout = 1; break; }
        if (!din) setbit(m, k);
        if (!din && !dout) { for (long j = k + 1; j <= K; j++) setbit(m, j); return 0; }   // 窓全体が {0, 2}（位置 1 は 1）: 以後も塊は汚れない
    }
    return 0;
}

// ---- 本番経路（8 ビット）: 値を半分にして持つ（0, 2 → 0, 1。2 以上が例外）。first でなく、窓の差が 510 以下の塊で使う ----
// 段 A: 窓全体を密に計算する（最初の行は 96% が例外なので run の管理は無駄）。ビット列は持たない。
//        {0, 1} が 32 セル以上（32 の境界に揃った組）続く所が現れたら段 B へ。
// 段 B: run（例外を含む連続区間）だけ計算し、run の外は 1 ビットの xor で進める。run の端を刈り、{0, 1} が T 個以上続く所で分割する。
typedef struct { uint64_t *b0, *b1; uint8_t *v0, *v1; int32_t *r0, *r1; } work_t;

static inline unsigned lanes32(long zz, long j) { return zz - j >= 32 ? 0xFFFFFFFFu : (1u << (zz - j)) - 1; }   // 組 [j, j+32) のうち zz 未満のレーン
// 32 セルの組の値 d から、2 のビット（値 1）と例外（2 以上）のマスクを作る
#define MASKS32(d, bm, em) do { bm = (unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8((d), one)); \
                                em = ~(unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_and_si256((d), msk), z)); } while (0)
#define ABSDIFF(v, j) _mm256_or_si256(_mm256_subs_epu8(_mm256_loadu_si256((const __m256i *)((v) + (j) + 1)), _mm256_loadu_si256((const __m256i *)((v) + (j)))), \
                                      _mm256_subs_epu8(_mm256_loadu_si256((const __m256i *)((v) + (j))), _mm256_loadu_si256((const __m256i *)((v) + (j) + 1))))
// 段 A の 1 行: nv[j] = |v[j+1] - v[j]|（j < Ln）。戻り値: {0, 1} だけの 32 セル組があるか
static inline int dense_step(const uint8_t *v, uint8_t *nv, long Ln) {
    const __m256i msk = _mm256_set1_epi8((char)0xFE), z = _mm256_setzero_si256(), one = _mm256_set1_epi8(1);
    int zero = 0; unsigned bm, em;
    (void)one;
    for (long j = 0; j < Ln; j += 32) {
        __m256i d = ABSDIFF(v, j);
        _mm256_storeu_si256((__m256i *)(nv + j), d);
        MASKS32(d, bm, em); (void)bm;
        zero |= (em & lanes32(Ln, j)) == 0;
    }
    return zero;
}
// ビット列 nb のセル j.. に 64 ビット分を書く（vm のレーンだけ）。語をまたぐことがある
static inline void write_bits64(uint64_t *nb, long j, uint64_t m, uint64_t vm) {
    long wd = j >> 6; int sh = (int)(j & 63);
    nb[wd] = (nb[wd] & ~(vm << sh)) | ((m & vm) << sh);
    if (sh) nb[wd + 1] = (nb[wd + 1] & ~(vm >> (64 - sh))) | ((m & vm) >> (64 - sh));
}
// 1 本の run の 1 行分（64 セルずつ）: [a, zz) の新しい値を nv に計算し（compute = 0 なら nv の値をそのまま使う）、その範囲のビットを nb に書き、
// 例外の塊から新しい run を NR に足す（直前の run と接していれば結合、{0, 1} が T 個以上続けば分割）。戻り値: 処理した 64 セル組の数
static inline long run_step(const uint8_t *v, uint8_t *nv, long a, long zz, uint64_t *nb, int32_t *NR, long *pnnr, int compute) {
    const __m256i msk = _mm256_set1_epi8((char)0xFE), z = _mm256_setzero_si256(), one = _mm256_set1_epi8(1);
    const long Tl = T;
    long nnr = *pnnr, cur = -1, last = -1, ng = 0;
#define EMIT(lo_, hi_) do { if (nnr > 0 && (lo_) <= NR[2 * nnr - 1]) { if ((hi_) > NR[2 * nnr - 1]) NR[2 * nnr - 1] = (int32_t)(hi_); } \
                            else { NR[2 * nnr] = (int32_t)(lo_); NR[2 * nnr + 1] = (int32_t)(hi_); nnr++; } } while (0)
    for (long j = a; j < zz; j += 64, ng++) {
        __m256i d0, d1;
        if (compute) {
            d0 = ABSDIFF(v, j); d1 = ABSDIFF(v, j + 32);
            _mm256_storeu_si256((__m256i *)(nv + j), d0); _mm256_storeu_si256((__m256i *)(nv + j + 32), d1);
        } else { d0 = _mm256_loadu_si256((const __m256i *)(nv + j)); d1 = _mm256_loadu_si256((const __m256i *)(nv + j + 32)); }
        unsigned b0, e0, b1, e1; MASKS32(d0, b0, e0); MASKS32(d1, b1, e1);
        uint64_t vm = zz - j >= 64 ? ~0ULL : (1ULL << (zz - j)) - 1;
        uint64_t bm = (uint64_t)b0 | (uint64_t)b1 << 32, em = ((uint64_t)e0 | (uint64_t)e1 << 32) & vm;
        write_bits64(nb, j, bm, vm);                                   // run の中のセルのビットは使われない（書いても害はない）
        if (em) {
            long pf = j + __builtin_ctzll(em), pl = j + 63 - __builtin_clzll(em);
            if (cur < 0) cur = pf;
            else if (Tl > 0 && pf - last - 1 >= Tl) { EMIT(cur, last + 1); cur = pf; }
            last = pl;
        }
    }
    if (cur >= 0) EMIT(cur, last + 1);
#undef EMIT
    *pnnr = nnr;
    return ng;
}
// 塊の処理。w[0..L) は行 1（素数の差、すべて偶数で 510 以下）。nb 個の位置が塊（残りは窓の延長）。
// m に、塊が {0, 2} だけの行 k のビットを立てる。
// 段 B の不変条件: run は昇順・互いに 1 セル以上離れ、両端のセルは例外。run の外のセルのビットは正しい。
static void block_runs8(const uint16_t *w, long L, long nb, seg_t *st, uint64_t *m, work_t *ws) {
    memset(m, 0, sizeof(uint64_t) * KW);
    uint64_t *b = ws->b0, *nbits = ws->b1; uint8_t *v = ws->v0, *nv = ws->v1; int32_t *R = ws->r0, *NR = ws->r1;
    const __m256i msk = _mm256_set1_epi8((char)0xFE), z = _mm256_setzero_si256();
    for (long j = 0; j < L; j += 32) {                                 // 半分にして 8 ビットに詰める
        __m256i lo = _mm256_srli_epi16(_mm256_loadu_si256((const __m256i *)(w + j)), 1), hi = _mm256_srli_epi16(_mm256_loadu_si256((const __m256i *)(w + j + 16)), 1);
        _mm256_storeu_si256((__m256i *)(v + j), _mm256_permute4x64_epi64(_mm256_packus_epi16(lo, hi), 0xD8));
    }
    long Lk = L, k = 1;
    for (;;) {                                                        // 段 A
        st->rows += (uint64_t)Lk;
        long r = nb < Lk ? nb : Lk; int dirty = 0;
        for (long j = 0; j < r && !dirty; j += 32)                      // 塊 [0, r) に例外があるか
            dirty = (~(unsigned)_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_and_si256(_mm256_loadu_si256((const __m256i *)(v + j)), msk), z)) & lanes32(r, j)) != 0;
        if (!dirty) setbit(m, k);
        if (k == K) return;
        long Ln = Lk - 1;
        int zero = dense_step(v, nv, Ln); st->cells += (uint64_t)Ln; st->arows++;
        uint8_t *tv = v; v = nv; nv = tv; Lk = Ln; k++;
        if (zero) break;
    }
    // 段 B の準備: いまの行 v から run とビット列を作る
    long nwords_b = (L >> 6) + 4;
    memset(b, 0, sizeof(uint64_t) * nwords_b); memset(nbits, 0, sizeof(uint64_t) * nwords_b);
    long nr = 0; run_step(NULL, v, 0, Lk, b, R, &nr, 0);
    for (;;) {                                                        // 段 B
        st->rows += (uint64_t)Lk;
        long r = nb < Lk ? nb : Lk;                                    // 汚れの判定: run の左端は例外なので、最初の run の lo が [0, r) に入れば汚れ
        if (!(nr > 0 && R[0] < r)) setbit(m, k);
        if (nr == 0) { for (long j = k + 1; j <= K; j++) setbit(m, j); return; }   // 窓が {0, 2} だけ: 以後の行も塊は汚れない
        if (k == K) return;
        long Ln = Lk - 1, nwn = (Ln >> 6) + 1;
        if (!(nr == 1 && R[0] == 0 && R[1] == Lk))                     // 1 本の run が窓全体なら、ビット列の xor は要らない
            for (long j = 0; j < nwn; j++) nbits[j] = b[j] ^ ((b[j] >> 1) | (b[j + 1] << 63));
        nbits[nwn] = 0;
        nbits[Ln >> 6] &= (1ULL << (Ln & 63)) - 1;                     // 行の外のビットを消す（Ln が 64 の倍数ならその語は丸ごと外）
        long nnr = 0;
        for (long q = 0; q < nr; q++) {
            long lo = R[2 * q], hi = R[2 * q + 1];
            long a = lo > 0 ? lo - 1 : 0, zz = hi < Ln ? hi : Ln;      // 次の行で値が変わりうるセル [a, zz)
            if (a >= zz) continue;
            if (lo > 0) v[lo - 1] = (uint8_t)getbit(b, lo - 1);         // 区間の両隣（{0, 1}）の値をビットから戻す
            if (hi < Lk) v[hi] = (uint8_t)getbit(b, hi);
            st->bgroups += (uint64_t)run_step(v, nv, a, zz, nbits, NR, &nnr, 1);
            st->cells += (uint64_t)(zz - a); st->bruns++;
        }
        uint64_t *tb = b; b = nbits; nbits = tb; uint8_t *tv = v; v = nv; nv = tv; int32_t *tr = R; R = NR; NR = tr; nr = nnr; Lk = Ln; k++; st->brows++;
    }
}

static seg_t segment(uint64_t s, uint64_t e) {
    seg_t r; memset(&r, 0, sizeof r); memset(r.clean, 0xFF, sizeof r.clean);
    long cap = B + K;                                                 // 窓の差の個数（素数は cap + 1 個）
    uint16_t *g = xmalloc(sizeof(uint16_t) * (cap + 64)), *tmp = xmalloc(sizeof(uint16_t) * (cap + 64));   // tmp: block_ref 用（値を壊すので複製）
    work_t ws = { xmalloc(sizeof(uint64_t) * ((cap >> 6) + 8)), xmalloc(sizeof(uint64_t) * ((cap >> 6) + 8)),
                  xmalloc(cap + 128), xmalloc(cap + 128),
                  xmalloc(sizeof(int32_t) * (cap + 8)), xmalloc(sizeof(int32_t) * (cap + 8)) };
    uint64_t m[KW], m2[KW];
    primesieve_iterator it; primesieve_init(&it);
    uint64_t prev;                                                    // 窓の最初の素数（s 以上で最小）。以後、最後に窓に入れた素数
#ifdef GPU_SRC
    gpusrc *gs = NULL;
    if (USE_GPU && s >= (1ULL << 33)) { gs = gpusrc_open(s); if (!gs || gpusrc_next(gs, NULL, 0, &prev) < 0) { fprintf(stderr, "gpusrc error\n"); exit(3); } r.gpu = 1; }
    else
#endif
    {
        primesieve_jump_to(&it, s, e + 100000000ULL);
        prev = primesieve_next_prime(&it);                            // 以後 it.primes[it.i] が最後に使った素数
        if (prev == PRIMESIEVE_ERROR) { fprintf(stderr, "primesieve error\n"); exit(3); }
    }
    long n = 0, nlt = prev < e ? 1 : 0;                                // n: 窓の差の個数、nlt: 窓の素数のうち e 未満の個数（位置として使える数）
    uint64_t p0 = prev;                                               // 窓の先頭（位置 g[0]）の素数
    int first = (s == 0);
    for (;;) {
#ifdef GPU_SRC
        if (gs && n < cap) {                                           // 窓を満たす（GPU）: 差を受け取り、素数をたどって e 未満の個数を数える
            long want = cap - n;
            if (gpusrc_next(gs, g + n, want, NULL) != want) { fprintf(stderr, "gpusrc error\n"); exit(3); }
            uint64_t q = prev; long c = 0; unsigned bad = 0;
            for (long t = 0; t < want; t++) { uint16_t d = g[n + t]; bad |= (d == 0) | (d & 1); q += d; c += q < e; }   // 0 や奇数の差は起こりえない（gpusrc でも検査する）
            nlt += c; prev = q; n = cap;
            if (bad) r.fail |= F_GAP;
        }
#endif
        while (n < cap) {                                              // 窓を満たす: iterator のバッファからまとめて差を取る
            long i0 = (long)it.i + 1;
            if (i0 >= (long)it.size) {
                primesieve_generate_next_primes(&it);
                if (it.is_error || it.size == 0) { fprintf(stderr, "primesieve error\n"); exit(3); }
                i0 = 0;                                                // 生成直後は primes[0] がまだ使っていない素数
            }
            long cnt = (long)it.size - i0; if (cnt > cap - n) cnt = cap - n;
            const uint64_t *P = it.primes + i0;
            uint64_t gor = P[0] - prev; g[n] = (uint16_t)gor;
            for (long t = 1; t < cnt; t++) { uint64_t d = P[t] - P[t - 1]; gor |= d; g[n + t] = (uint16_t)d; }   // OR で 65535 超えを後で検出
            if (gor > 65535) { r.fail |= F_GAP; break; }
            if (P[cnt - 1] < e) nlt += cnt;
            else { long lo = 0, hi = cnt; while (lo < hi) { long mid = (lo + hi) / 2; if (P[mid] < e) lo = mid + 1; else hi = mid; } nlt += lo; }
            prev = P[cnt - 1]; it.i = (size_t)(i0 + cnt - 1); n += cnt;
        }
        if (r.fail) break;
        long nb = nlt < B ? nlt : B;                                   // 塊の位置数（e 未満の素数だけ）
        if (nb == 0) break;
        uint16_t wmax = 0; for (long i = 0; i < n; i++) if (g[i] > wmax) wmax = g[i];
        int use8 = !first && wmax <= 510;                              // 先頭の塊（差 1 を含む）と大きな差の塊は参照経路で
        if (SIEVE_ONLY) { memset(m, 0xFF, sizeof m); m[0] &= ~1ULL; }
        else if (use8) block_runs8(g, n, nb, &r, m, &ws);
        else { r.refblocks++; memcpy(tmp, g, sizeof(uint16_t) * n); int rc = block_ref(tmp, n, nb, first, m, &r.ce_val); if (rc < 0) { r.fail |= F_CE; r.ce_row = -rc; break; } }
        if (SELFCHECK && use8) { unsigned cv; memcpy(tmp, g, sizeof(uint16_t) * n); if (block_ref(tmp, n, nb, first, m2, &cv) < 0 || memcmp(m, m2, sizeof m) != 0) { r.fail |= F_SC; break; } }
        for (int j = 0; j < KW; j++) r.clean[j] &= m[j];
        {   // 塊の位置の素数の差の最大（公開表 A002386/A005250 との照合用）。最大の候補が出たときだけ位置を探す
            // （位置の素数の和も検算に持っていたが、Ir +0.8% に見合わないので外した。差の最大と和だけなら +0.35%）
            uint16_t bm = 0; uint64_t gsum = 0;
            for (long i = 0; i < nb; i++) { uint16_t x = g[i]; bm = x > bm ? x : bm; gsum += x; }
            if (bm > r.maxgap) { uint64_t q = p0; for (long i = 0; i < nb; i++) { if (g[i] == bm) { r.maxgap = bm; r.maxgap_p = q; break; } q += g[i]; } }
            p0 += gsum;
        }
        r.npos += (uint64_t)nb; first = 0;
        memmove(g, g + nb, sizeof(uint16_t) * (n - nb)); n -= nb; nlt -= nb;
    }
    if (!r.fail && r.npos == 0 && W >= 4096) r.fail |= F_NOP;            // 素数が 1 つも無い区間は primesieve の失敗を疑う（2^64 未満の素数の差は 1550 未満なので、幅 4096 以上の区間には必ず素数がある）
    if (!r.fail && SELFCHECK && r.npos != primesieve_count_primes(s, e - 1)) r.fail |= F_SC;   // 素数の取り出しの照合（SELFCHECK のときだけ。篩をもう 1 回）                         // 素数が 1 つも無い区間は primesieve の失敗を疑う
    if (!r.fail && first_clean(r.clean) < 0) r.fail |= F_K;
#ifdef GPU_SRC
    if (gs) gpusrc_close(gs);
#endif
    primesieve_free_iterator(&it); free(g); free(tmp); free(ws.b0); free(ws.b1); free(ws.v0); free(ws.v1); free(ws.r0); free(ws.r1);
    return r;
}

static int nwords(void) { return (int)(K / 64) + 1; }
static void hex_clean(char *out, const uint64_t *c) { for (int j = 0; j < nwords(); j++) out += sprintf(out, "%016llx", (unsigned long long)c[j]); }

static uint64_t parse_u64(const char *s, uint64_t lo, uint64_t hi, const char *name) {
    char *end; errno = 0; unsigned long long v = strtoull(s, &end, 10);
    if (*s < '0' || *s > '9' || errno || *end || v < lo || v > hi) { fprintf(stderr, "bad %s: '%s'\n", name, s); exit(1); }
    return v;
}

// DONE 行の解析: この実行の tag・区間割りに合い、clean= の 16 進がちょうど nwords()*16 桁で改行で終わる行だけ受け付ける
// （書きかけで切れた行を、欠けたビット列として読まないため）。
static int parse_done(const char *line, const char *tag, uint64_t nseg, uint64_t *pi, uint64_t *pnpos, uint64_t *c) {
    unsigned long long i, s0, e0, np; int off = 0;
    size_t tl = strlen(tag);
    if (sscanf(line, "DONE %llu %llu %llu %n", &i, &s0, &e0, &off) != 3 || !off || strncmp(line + off, tag, tl) || line[off + tl] != '|') return 0;
    if (i >= nseg || s0 != i * W || e0 != s0 + W) return 0;
    const char *p = strstr(line, " npos="), *h = strstr(line, " clean=");
    if (!p || !h || sscanf(p, " npos=%llu", &np) != 1) return 0;
    h += 7;
    size_t n = (size_t)nwords() * 16;
    if (strspn(h, "0123456789abcdef") != n || strcmp(h + n, "\n") != 0) return 0;
    memset(c, 0, sizeof(uint64_t) * KW);
    for (int j = 0; j < nwords(); j++) { char t16[17]; memcpy(t16, h + 16 * j, 16); t16[16] = 0; c[j] = strtoull(t16, NULL, 16); }
    *pi = i; *pnpos = np;
    return 1;
}

// 改行の確認をしてから 1 行を追記し、fsync する（書きかけの行に連結しない）。失敗は終了コード 3。
static void append_line(const char *logf, const char *text) {
    FILE *f = fopen(logf, "a+");
    if (!f) { perror(logf); exit(3); }
    int last = '\n';
    if (fseek(f, 0, SEEK_END) == 0 && ftell(f) > 0 && fseek(f, -1, SEEK_END) == 0) last = fgetc(f);
    fseek(f, 0, SEEK_END);
    if (last != '\n') fputc('\n', f);
    fputs(text, f);
    if (ferror(f) || fflush(f) || fsync(fileno(f)) || fclose(f)) { perror(logf); exit(3); }
}

static const char *reason(int f) {
    return f & F_CE ? "counterexample_candidate" : f & F_GAP ? "gap_overflow" : f & F_NOP ? "no_primes" : f & F_SC ? "selfcheck_mismatch" : f & F_K ? "K_too_small" : "none";
}

// 終了コード: 0 = OK、1 = 引数、2 = 反例候補、3 = 入出力・メモリ、4 = 計測用の実行、5 = K 不足、6 = SELFCHECK 不一致・差のあふれ、
//             7 = 区間が欠けている・同じ区間の行が食い違う・FAIL 行が残っている、8 = 同じ log を使う別の実行が動いている、9 = 信号で停止（再実行で続きから）
//             4 は BENCH_LO / SIEVE_ONLY / RECHECK の実行（検証ではない）、6 には区間に素数が無い場合（primesieve の失敗の疑い）も含む
static int exit_code(int f) { return f & F_CE ? 2 : f & (F_SC | F_GAP | F_NOP) ? 6 : f & F_K ? 5 : 0; }

int main(int argc, char **argv) {
    if (argc != 6 && argc != 7) { fprintf(stderr, "usage: gilbreath X K B W T [logfile]\n"); return 1; }
    X = parse_u64(argv[1], 1, (uint64_t)1 << 62, "X");
    K = (long)parse_u64(argv[2], 1, 64 * KW - 1, "K");
    B = (long)parse_u64(argv[3], 1, 1 << 24, "B");
    W = parse_u64(argv[4], 3, X, "W");                               // W >= 3: 素数 2 が区間 0 に入る（位置 1 の扱い）
    T = (long)parse_u64(argv[5], 0, 1 << 20, "T");
    if (X % W) { fprintf(stderr, "X must be a multiple of W\n"); return 1; }
    SELFCHECK = getenv("SELFCHECK") && atoi(getenv("SELFCHECK"));
    SIEVE_ONLY = getenv("SIEVE_ONLY") && atoi(getenv("SIEVE_ONLY"));
    USE_GPU = getenv("GPU") && atoi(getenv("GPU"));
#ifdef GPU_SRC
    if (USE_GPU) {
        if (W & 1) { fprintf(stderr, "GPU needs an even W\n"); return 1; }
        if (gpusrc_global_init(X + 100000000ULL)) { fprintf(stderr, "gpusrc init failed\n"); return 3; }
    }
#else
    if (USE_GPU) { fprintf(stderr, "GPU=1 needs a build with -DGPU_SRC\n"); return 1; }
#endif
    if (SIEVE_ONLY && !getenv("BENCH_LO")) { fprintf(stderr, "SIEVE_ONLY needs BENCH_LO (not a verification)\n"); return 1; }
    primesieve_set_sieve_size(getenv("SIEVE_KIB") ? (int)parse_u64(getenv("SIEVE_KIB"), 16, 8192, "SIEVE_KIB") : 32);   // 32 KiB が最速（実測）
    const char *logf = argc == 7 ? argv[6] : NULL;
    uint64_t nseg = X / W, i0 = 0; int bench = SIEVE_ONLY;
    char *only = NULL;                                                // RECHECK: 指定した区間だけ計算し直す（log は本番と別のファイル。照合用）
    uint64_t seg_lo = 0, seg_hi = nseg;                               // SEG_LO/SEG_HI: この実行が受け持つ区間の番号の範囲 [lo, hi)（複数の計算機で分担するとき。log は後で結合して再実行すると ALLDONE になる）
    if (getenv("SEG_LO")) seg_lo = parse_u64(getenv("SEG_LO"), 0, nseg - 1, "SEG_LO");
    if (getenv("SEG_HI")) seg_hi = parse_u64(getenv("SEG_HI"), seg_lo + 1, nseg, "SEG_HI");
    if ((getenv("SEG_LO") || getenv("SEG_HI")) && !logf) { fprintf(stderr, "SEG_LO/SEG_HI need a logfile (a partial run alone is not a verification)\n"); return 1; }
    if (getenv("BENCH_LO")) {                                        // 計測用: [BENCH_LO, X) の区間だけ（検証にならない。log とは併用不可）
        if (logf) { fprintf(stderr, "BENCH_LO cannot be used with logfile\n"); return 1; }
        i0 = parse_u64(getenv("BENCH_LO"), 0, X - 1, "BENCH_LO") / W; bench = 1;
    }
    if (getenv("RECHECK")) {
        only = calloc(nseg, 1); if (!only) { fprintf(stderr, "out of memory\n"); return 3; }
        char *list = strdup(getenv("RECHECK"));
        for (char *t = strtok(list, ","); t; t = strtok(NULL, ",")) only[parse_u64(t, 0, nseg - 1, "RECHECK segment")] = 1;
        free(list);
    }
    char tag[256];
    snprintf(tag, sizeof tag, "K=%ld B=%ld W=%llu T=%ld sc=%d src=%s", K, B, (unsigned long long)W, T, SELFCHECK, SRC_HASH);
    char *done = calloc(nseg, 1); if (!done) { fprintf(stderr, "out of memory\n"); return 3; }
    if (logf) {
        char lk[4096]; snprintf(lk, sizeof lk, "%s.lock", logf);     // 同じ log に 2 つの実行が書かないように（終了まで保持）
        int lockfd = open(lk, O_CREAT | O_RDWR, 0644);
        if (lockfd < 0) { perror(lk); return 3; }
        if (flock(lockfd, LOCK_EX | LOCK_NB)) { fprintf(stderr, "another run holds %s\n", lk); return 8; }
        FILE *f = fopen(logf, "r");                                  // 再開: 同じ tag の DONE 行の区間を飛ばす
        if (!f && errno != ENOENT) { perror(logf); return 3; }
        if (f) {
            static char line[4096];
            uint64_t i, np, c[KW];
            while (fgets(line, sizeof line, f)) if (parse_done(line, tag, nseg, &i, &np, c)) done[i] = 1;
            fclose(f);
        }
    }
    uint64_t npos = 0, rows = 0, cells = 0, refb = 0, arows = 0, brows = 0, bruns = 0, bgroups = 0, nproc = 0; int fail = 0, stop = 0;
    { struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = on_signal; sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL); }
    if (logf) {                                                       // 再現性のために版を残す（DONE 行ではないので集計には使われない）
        char info[512]; snprintf(info, sizeof info, "# versions primesieve=%s gcc=%s tag=%s\n", primesieve_version(), __VERSION__, tag);
        append_line(logf, info);
    }
    uint64_t clean[KW]; memset(clean, 0xFF, sizeof clean);
    double t0 = omp_get_wtime();
    #pragma omp parallel for schedule(dynamic, 1) reduction(+:npos, rows, cells, refb, arows, brows, bruns, bgroups, nproc) reduction(|:fail)
    for (uint64_t i = i0; i < nseg; i++) {
        int st;
        #pragma omp atomic read
        st = stop;
        if (st || stop_req || (only ? !only[i] : done[i])) continue;  // stop: どこかで失敗したら残りを始めない。stop_req: 停止の要求（進行中の区間は終える）
        if (i < seg_lo || i >= seg_hi) continue;
        double ts = omp_get_wtime();
        uint64_t s = i * W, e = s + W;
        seg_t r = segment(s, e); nproc++;
        npos += r.npos; rows += r.rows; cells += r.cells; refb += r.refblocks; arows += r.arows; brows += r.brows; bruns += r.bruns; bgroups += r.bgroups; fail |= r.fail;
        if (r.fail) {
            #pragma omp atomic write
            stop = 1;
        }
        #pragma omp critical
        {
            for (int j = 0; j < KW; j++) clean[j] &= r.clean[j];
            if (logf) {
                char hx[64 * KW + 1]; hex_clean(hx, r.clean);
                static char text[8192];
                snprintf(text, sizeof text, "%s %llu %llu %llu %s| npos=%llu G=%ld maxgap=%u@%llu rows=%llu cells=%llu refblocks=%llu time=%.3fs reason=%s ce_row=%ld ce_val=%u sieve=%s clean=%s\n",
                         r.fail ? "FAIL" : "DONE", (unsigned long long)i, (unsigned long long)s, (unsigned long long)e, tag, (unsigned long long)r.npos,
                         first_clean(r.clean), r.maxgap, (unsigned long long)r.maxgap_p, (unsigned long long)r.rows, (unsigned long long)r.cells, (unsigned long long)r.refblocks, omp_get_wtime() - ts, reason(r.fail), r.ce_row, r.ce_val, r.gpu ? "gpu" : "cpu", hx);
                append_line(logf, text);
            }
        }
    }
    double dt = omp_get_wtime() - t0;
    printf("this run: segs=%llu pi_part=%llu fail=%s rows/pos=%.2f cells/pos=%.2f refblocks=%llu arows=%llu brows=%llu bruns=%llu bgroups=%llu time=%.3fs\n",
           (unsigned long long)nproc, (unsigned long long)npos, reason(fail), npos ? (double)rows / (double)npos : 0, npos ? (double)cells / (double)npos : 0,
           (unsigned long long)refb, (unsigned long long)arows, (unsigned long long)brows, (unsigned long long)bruns, (unsigned long long)bgroups, dt);
    if (fail) { printf("FAILED (%s)\n", reason(fail)); return exit_code(fail); }
    if (bench) { printf("BENCH run: NOT a verification\n"); return 4; }
    if (stop_req) { printf("stopped by signal after finishing the segments in progress\n"); return 9; }
    if (only) { printf("RECHECK run: compare its DONE lines with the main log\n"); return 4; }
    if (!logf) {
        long G = first_clean(clean);
        printf("X=%llu pi=%llu G=%ld %s\n", (unsigned long long)X, (unsigned long long)npos, G, G > 0 ? "OK" : "K too small");
        return G > 0 ? 0 : 5;
    }
    // 全区間の集計は log から（再開前の分も含む）。同じ区間の行が食い違えば失敗（ハードウェアの誤りの手がかり）
    FILE *f = fopen(logf, "r"); if (!f) { perror(logf); return 3; }
    memset(done, 0, nseg); memset(clean, 0xFF, sizeof clean); npos = 0;
    uint64_t (*seen)[KW + 1] = calloc(nseg, sizeof *seen); if (!seen) { fprintf(stderr, "out of memory\n"); return 3; }
    static char line[4096]; uint64_t nseen = 0, nconf = 0, nfail = 0;   // nfail: 同じ tag の FAIL 行（再実行で DONE になっても記録に残す）
    uint64_t i, np, c[KW];
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "FAIL ", 5) == 0 && strstr(line, tag)) nfail++;
        if (!parse_done(line, tag, nseg, &i, &np, c)) continue;
        if (done[i]) { if (seen[i][KW] != np || memcmp(seen[i], c, sizeof c)) nconf++; continue; }
        memcpy(seen[i], c, sizeof c); seen[i][KW] = np;
        for (int j = 0; j < KW; j++) clean[j] &= c[j];
        done[i] = 1; npos += np; nseen++;
    }
    fclose(f); free(seen);
    long G = first_clean(clean);
    int ok = nseen == nseg && nconf == 0 && nfail == 0 && G > 0;    // 同じ tag の FAIL 行が残っていれば OK にしない（決定的な計算では矛盾 = ハードウェアの誤りの手がかり。RECHECK で確かめる）
    const char *why = nconf ? "CONFLICT" : nfail ? "FAIL_LINES" : nseen != nseg ? "INCOMPLETE" : G <= 0 ? "K_TOO_SMALL" : "OK";
    char text[1024];
    snprintf(text, sizeof text, "%s X=%llu %s| segs=%llu/%llu pi=%llu G=%ld conflicts=%llu fail_lines=%llu %s\n", ok ? "ALLDONE" : "ALLFAIL", (unsigned long long)X, tag,
             (unsigned long long)nseen, (unsigned long long)nseg, (unsigned long long)npos, G, (unsigned long long)nconf, (unsigned long long)nfail, why);
    append_line(logf, text);
    printf("%s", text);
    return ok ? 0 : (G <= 0 && nseen == nseg && !nconf && !nfail) ? 5 : 7;
}
