# Latency notes

Measured with `mercury_bench` (Google Benchmark), Release build, Clang 22,
Windows host (16 x 4700 MHz). Values are wall-clock nanoseconds, 20
repetitions, aggregates only.

| Path | median | p95 | p99 |
| --- | ---: | ---: | ---: |
| Rest limit | 219 | 230 | 230 |
| Match 1-lot limit | 238 | 257 | 257 |
| Cancel | 158 | 162 | 162 |

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target mercury_bench
./build-release/benchmarks/mercury_bench
```
