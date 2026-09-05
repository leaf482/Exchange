# Roadmap

Incremental work only. Correctness and measurement before concurrency.

## Done

- Limit / market / cancel matching (price-time priority, partial fills)
- IOC / FOK time-in-force on limit orders (GTC default)
- Stop orders (last-trade trigger → limit or market)
- Multi-instrument books keyed by `Symbol` (positions/risk per symbol)
- OrderId index, positions + PnL, pre-trade risk (incl. resting exposure)
- Deterministic event log + JSONL save/load + replay (`jsonl_replay`)
- Book depth snapshot (`book_snapshot`)
- Latency benches (rest / match / cancel / deep book)
- Python generate / replay / C++ parity compare
- Python market sim (Bernoulli arrivals, inventory-skewed maker + takers)

## Next (small steps)

1. Optional pybind11 surface for `Engine` / `snapshot`

## Later

- Threading / shard-by-symbol only after single-thread benches justify it
- Networking, storage backends, UI — outside the matching core

## Not planned soon

Lock-free structures, Kafka/Redis, Docker-centric deploys, or rewriting the
engine for throughput before profiling a concrete bottleneck.
