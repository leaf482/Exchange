# Mercury Exchange

Deterministic, low-latency exchange simulator and trading engine in C++23.

## Current scope

Matching engine (limit/market/cancel), positions, pre-trade risk, event replay,
and a first latency benchmark.

## Build

Requires CMake 3.20+ and a C++23 compiler (GCC or Clang).

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/benchmarks/mercury_bench
```
