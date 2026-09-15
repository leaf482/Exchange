# Latency notes

Measured with `mercury_bench` (Google Benchmark), Release build, Clang,
Windows host (16 x 4700 MHz). Values are wall-clock nanoseconds.

Each benchmark runs with **20 Google Benchmark repetitions**
(`Repetitions(20)`). The columns below are statistics over those **20
repetition timings**, not percentiles over a large sample of individual order
operations. Treat them as:

- **median repetition** — median of the 20 repetition timings
- **repetition p95 / p99** — 95th / 99th percentile of the 20 repetition timings

| Path | median repetition | repetition p95 | repetition p99 |
| --- | ---: | ---: | ---: |
| Rest limit | 289 | 302 | 302 |
| Match 1-lot limit | 374 | 379 | 379 |
| Cancel | 188 | 190 | 190 |
| Match deep book (8 levels) | 954 | 980 | 980 |
| Match deep book (32 levels) | 3243 | 3284 | 3284 |
| Match deep book (128 levels) | 14690 | 15044 | 15044 |
| Match one of N symbols (1) | 378 | 387 | 387 |
| Match one of N symbols (8) | 885 | 907 | 907 |
| Match one of N symbols (32) | 3161 | 3245 | 3245 |
| Mass cancel account × symbols (8) | 1489 | 1510 | 1510 |
| Mass cancel account × symbols (32) | 6452 | 6513 | 6513 |
| Iceberg tip-refill (hidden 32, display 1) | 1228 | 1244 | 1244 |
| Iceberg tip-refill (hidden 128, display 1) | 3879 | 3919 | 3919 |
| Account report | 314 | 318 | 318 |

Deep-book cases seed N ask levels (1 lot each) and sweep them with one buy.
Multi-symbol match seeds one resting ask per symbol (untimed) and crosses
symbol 0. Mass cancel rests two buys per symbol (accounts 1 and 2), then
cancels account 1. Iceberg cases rest one sell with `display=1` and take the
full hidden size (tip refill + requeue each lot).

## Shard decision

Single-thread match on one of 32 symbols is ~3 µs **median repetition**. Cost
grows with instrument map lookups / book depth, not with a contended shared
lock. No hotspot yet that justifies shard-by-symbol threading; keep measuring
before adding concurrency.

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target mercury_bench
./build-release/benchmarks/mercury_bench
```
