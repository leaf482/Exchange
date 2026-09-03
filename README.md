# Mercury Exchange

Deterministic, low-latency exchange simulator and trading engine in C++23.

## Current scope

Matching engine (limit/market/cancel), positions, pre-trade risk, event replay,
latency benchmarks, and a Python event generator/analyzer.

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

## Python

Stdlib only. From `python/`:

```bash
python -m mercury_sim.generate -n 100 --seed 1 -o events.jsonl
python -m mercury_sim.analyze events.jsonl
python -m unittest discover -s tests
```
