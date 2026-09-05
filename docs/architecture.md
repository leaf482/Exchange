# Architecture

Single-threaded, deterministic matching core. Prices and quantities are integers
(ticks / lots). No networking or persistence in the hot path.

## Layers

```
apps/jsonl_replay          CLI: JSONL events -> trades
python/mercury_sim         generate / market sim / replay / compare
python/mercury_engine      optional pybind11 Engine / snapshot
        |
        v
Engine                     risk -> per-Symbol OrderBook -> positions + working
                           (+ pending StopOrder until last trade triggers)
        |
        +-- OrderBook      bids/asks of PriceLevel (price-time priority)
        |                    snapshot(depth) -> BookSnapshot
        +-- Positions      signed qty / PnL per (account, symbol)
        +-- RiskLimits     max order size, max abs position (per symbol)
        |
EventLog / jsonl           save/load JSONL for exact replay (limit/market/cancel)
```

## Matching

- Limit: match opposite side while prices cross; GTC rests remainder,
  IOC discards remainder, FOK requires a full immediate fill or rejects.
- Market: match available liquidity, discard unfilled qty.
- Stop: armed until last trade crosses `stop_price` (buy `>=`, sell `<=`),
  then becomes limit (`limit_price`) or market; same id; cancel removes pending.
- Cancel: remove resting order / pending stop by `OrderId` (routed by symbol).
- Instruments are isolated: orders and last-trade stops never cross symbols.
- Trade price is the maker (resting) price.
- Persistence: `jsonl::save_event_log_file` / `load_event_log_file` round-trip
  the event stream (including `tif` / `symbol`); replay is deterministic.

## Repo layout

| Path | Role |
| --- | --- |
| `include/mercury/` | public headers (header-mostly library) |
| `src/` | small compiled pieces (`version`) |
| `tests/` | GoogleTest |
| `benchmarks/` | Google Benchmark latency |
| `apps/` | executables |
| `python/` | simulation and C++/Python parity |
| `docs/` | design / latency notes |

## Non-goals (for now)

Multithreading, lock-free structures, networking, databases, and Python
bindings to the C++ engine. Correctness and measurement come first.
