# Latency notes

Measured with `mercury_bench` (Google Benchmark), Release build, Clang 22,
Windows host (16 x 4700 MHz). Values are wall-clock nanoseconds, 20
repetitions, aggregates only.

| Path | median | p95 | p99 |
| --- | ---: | ---: | ---: |
| Rest limit | 272 | 297 | 297 |
| Match 1-lot limit | 259 | 270 | 270 |
| Cancel | 176 | 184 | 184 |
| Match deep book (8 levels) | 893 | 943 | 943 |
| Match deep book (32 levels) | 3234 | 3284 | 3284 |
| Match deep book (128 levels) | 13726 | 13921 | 13921 |

Deep-book cases seed N ask levels (1 lot each) and sweep them with one buy.

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target mercury_bench
./build-release/benchmarks/mercury_bench
```
