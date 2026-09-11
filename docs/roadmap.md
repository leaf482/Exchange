# Roadmap

Incremental work only. Correctness and measurement before concurrency.

## Progress (portfolio core)

Matching / risk / accounting for a single-threaded exchange sim is largely in
place (~80% of the intended core). Remaining work is polish, optional product
features, and anything outside the matching path (net/UI) — not required for
correctness.

| Area | Status |
| --- | --- |
| Matching (limit/market/cancel, TIF, stop, replace, mass cancel) | Done |
| Risk (size/position, STP, post-only, reduce-only, cash+reserve) | Done |
| Positions / PnL / fees / TradeId | Done |
| Deterministic JSONL + Engine replay + Python parity | Done |
| Latency benches (single-thread) | Done |
| Networking / UI / multi-thread shards | Not started (deferred) |

## Done

- Limit / market / cancel matching (price-time priority, partial fills)
- IOC / FOK time-in-force on limit orders (GTC default)
- Stop orders (last-trade trigger → limit or market)
- Multi-instrument books keyed by `Symbol` (positions/risk per symbol)
- OrderId index, positions + PnL, pre-trade risk (incl. resting exposure)
- Deterministic event log + JSONL save/load + Engine replay (incl. stops)
- Book depth snapshot (`book_snapshot`)
- Latency benches (rest / match / cancel / deep book / multi-symbol / mass cancel)
- Python generate / replay / C++ parity compare (tif + stop)
- Python market sim (Bernoulli arrivals, inventory-skewed maker + takers)
- Optional pybind11 `mercury_engine` module (`Engine` / `snapshot`)
- Self-trade prevention (`CancelResting`; account 0 exempt; default off)
- Order replace (cancel-replace; loses time priority; JSONL `replace`)
- Mass cancel by account / symbol / side (JSONL `mass_cancel`)
- Maker/taker fees in bps (tick×qty units; optional rebates)
- Post-only limits (`post_only`; Engine `RiskDecision::PostOnly`)
- Reduce-only limits/markets (no open / increase / flip; `RiskDecision::ReduceOnly`)
- Unrealized PnL marked at last trade or mid (`MarkSource`; default last trade)
- Monotonic `TradeId` stamped by Engine (OrderBook leaves 0)
- Account cash ledger (tick×qty); optional buy cash enforcement + resting reserve
- Reject audit events (JSONL `reject`; replay no-op)
- Iceberg limits (`display` peak; snapshot hides remainder; matching uses full qty)

## Next (small steps)

1. Optional product extras (GTD expire, sell/short margin)
2. Shard-by-symbol matching only after benches show a hotspot
3. Optional networking / storage / UI outside the matching core

## Later

- Threading / shard-by-symbol only after single-thread benches justify it
- Networking, storage backends, UI — outside the matching core

## Not planned soon

Lock-free structures, Kafka/Redis, Docker-centric deploys, or rewriting the
engine for throughput before profiling a concrete bottleneck.
