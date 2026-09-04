# Architecture

Single-threaded, deterministic matching core. Prices and quantities are integers
(ticks / lots). No networking or persistence in the hot path.

## Layers

```
apps/jsonl_replay          CLI: JSONL events -> trades
python/mercury_sim         generate / replay / compare (parity with C++)
        |
        v
Engine                     risk check -> OrderBook -> positions + working exposure
                           (+ pending StopOrder until last trade triggers)
        |
        +-- OrderBook      bids/asks of PriceLevel (price-time priority)
        |                    snapshot(depth) -> BookSnapshot
        +-- Positions      signed qty, realized / unrealized PnL
        +-- RiskLimits     max order size, max abs position (incl. resting)
        |
EventLog / jsonl           input events: limit, market, cancel
```

## Matching

- Limit: match opposite side while prices cross; GTC rests remainder,
  IOC discards remainder, FOK requires a full immediate fill or rejects.
- Market: match available liquidity, discard unfilled qty.
- Stop: armed until last trade crosses `stop_price` (buy `>=`, sell `<=`),
  then becomes limit (`limit_price`) or market; same id; cancel removes pending.
- Cancel: remove resting order by `OrderId` (or pending stop).
- Trade price is the maker (resting) price.

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
