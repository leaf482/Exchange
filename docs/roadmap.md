# Roadmap

Incremental work only. Correctness and measurement before concurrency.

## Done

- Limit / market / cancel matching (price-time priority, partial fills)
- IOC / FOK time-in-force on limit orders (GTC default)
- Stop orders (last-trade trigger → limit or market)
- Multi-instrument books keyed by `Symbol` (positions/risk per symbol)
- OrderId index, positions + PnL, pre-trade risk (incl. resting exposure)
- Deterministic event log + JSONL save/load + Engine replay (incl. stops)
- Book depth snapshot (`book_snapshot`)
- Latency benches (rest / match / cancel / deep book)
- Python generate / replay / C++ parity compare (tif + stop)
- Python market sim (Bernoulli arrivals, inventory-skewed maker + takers)
- Optional pybind11 `mercury_engine` module (`Engine` / `snapshot`)
- Self-trade prevention (`CancelResting`; account 0 exempt; default off)
- Order replace (cancel-replace; loses time priority; JSONL `replace`)

## Next (small steps)

1. Shard-by-symbol matching only after benches show a hotspot
2. Optional networking / storage / UI outside the matching core

## Later

- Threading / shard-by-symbol only after single-thread benches justify it
- Networking, storage backends, UI — outside the matching core

## Not planned soon

Lock-free structures, Kafka/Redis, Docker-centric deploys, or rewriting the
engine for throughput before profiling a concrete bottleneck.
