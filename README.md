# Mercury Exchange

Deterministic, low-latency exchange simulator and trading engine in C++23.

## Current scope

Matching engine (limit/market/cancel), positions, pre-trade risk, event replay,
latency benchmarks, and a Python market simulator / analyzer.

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

See `docs/architecture.md` for module layout, `docs/latency.md` for recorded
numbers, and `docs/roadmap.md` for next steps.

## Python

Stdlib only. From `python/`:

```bash
python -m mercury_sim.generate -n 100 --seed 1 -o events.jsonl
python -m mercury_sim.generate -n 100 --mode market --seed 1 -o market.jsonl
python -m mercury_sim.analyze events.jsonl
python -m mercury_sim.replay events.jsonl
python -m mercury_sim.compare events.jsonl
python -m unittest discover -s tests
```

Optional C++ Engine bindings (pybind11):

```bash
cmake -S . -B build -DMERCURY_BUILD_PYTHON=ON
cmake --build build --target mercury_engine
# module lands in python/mercury_engine.*
python -c "import mercury_engine; e=mercury_engine.Engine(); print(e.snapshot())"
```

Replay the same JSONL with the C++ engine:

```bash
./build/apps/jsonl_replay events.jsonl
./build/apps/book_snapshot events.jsonl 5
```
