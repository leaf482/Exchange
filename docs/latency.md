# Latency notes

Measured with `mercury_bench` (Google Benchmark), Release build, Clang 22,
Windows host (16 x 4700 MHz). Values are wall-clock nanoseconds, 20
repetitions, aggregates only.

| Path | median | p95 | p99 |
| --- | ---: | ---: | ---: |
| Rest limit | 443 | 564 | 564 |
| Match 1-lot limit | 509 | 559 | 559 |
| Cancel | 272 | 332 | 332 |
| Match deep book (8 levels) | 1649 | 2150 | 2150 |
| Match deep book (32 levels) | 5834 | 6195 | 6195 |
| Match deep book (128 levels) | 20743 | 27558 | 27558 |
| Match one of N symbols (1) | 432 | 491 | 491 |
| Match one of N symbols (8) | 1288 | 1677 | 1677 |
| Match one of N symbols (32) | 3751 | 5015 | 5015 |
| Mass cancel account × symbols (8) | 1914 | 1990 | 1990 |
| Mass cancel account × symbols (32) | 8774 | 12096 | 12096 |

Deep-book cases seed N ask levels (1 lot each) and sweep them with one buy.
Multi-symbol match seeds one resting ask per symbol and crosses symbol 0.
Mass cancel rests two buys per symbol (accounts 1 and 2), then cancels account 1.

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target mercury_bench
./build-release/benchmarks/mercury_bench
```
