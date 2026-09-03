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
```

Release latency benchmark:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target mercury_bench
./build-release/benchmarks/mercury_bench
```

See `docs/latency.md` for recorded numbers.
