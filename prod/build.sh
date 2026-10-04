#!/bin/bash
# 本番用バイナリをコンテナ内でビルドする（/w = このフォルダ）。ソースの md5 を tag に埋める。
cd /w || exit 1
H=$(md5sum gilbreath.c | cut -c1-12)
mkdir -p prod
gcc -O3 -march=native -fopenmp -Wall -Wextra "-DSRC_HASH=\"$H\"" -o prod/gilbreath_prod gilbreath.c -lprimesieve || exit 1
echo "built prod/gilbreath_prod src=$H md5=$(md5sum prod/gilbreath_prod | cut -c1-12)"
