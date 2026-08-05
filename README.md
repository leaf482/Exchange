# Mercury Exchange

Deterministic, low-latency exchange simulator and trading engine in C++23.

## Current scope

CMake library target, GoogleTest smoke test, CTest. No matching engine yet.

## Build

Requires CMake 3.20+ and a C++23 compiler (GCC or Clang).

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
