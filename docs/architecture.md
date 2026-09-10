# Architecture

Single-threaded, deterministic matching core. Prices and quantities are integers
(ticks / lots). No networking or persistence in the hot path.

## Layers

```
apps/jsonl_replay          CLI: JSONL events -> trades
python/mercury_sim         generate / market sim / Engine replay / compare
python/mercury_engine      optional pybind11 Engine / snapshot
        |
        v
Engine                     risk -> per-Symbol OrderBook -> positions + working
                           (+ pending StopOrder until last trade triggers)
        |
        +-- OrderBook      bids/asks of PriceLevel (price-time priority)
        |                    snapshot(depth) -> BookSnapshot
        +-- Positions      signed qty / realized + unrealized (last-trade or mid mark)
        +-- Balances       cash (tick×qty); optional enforce on buys
        +-- RiskLimits     max order size, max abs position (per symbol)
        |
EventLog / jsonl           Engine replay (+ reject audit no-ops)
```

## Matching

- Limit: match opposite side while prices cross; GTC rests remainder,
  IOC discards remainder, FOK requires a full immediate fill or rejects.
  `post_only` rejects (no fill, no rest) if the limit would take liquidity.
  `reduce_only` rejects unless the order shrinks an existing position (no flip).
- Market: match available liquidity, discard unfilled qty.
- Stop: armed until last trade crosses `stop_price` (buy `>=`, sell `<=`),
  then becomes limit (`limit_price`) or market; same id; cancel removes pending.
- Cancel: remove resting order / pending stop by `OrderId` (routed by symbol).
- Self-trade prevention (optional): `CancelResting` drops same-account resting
  orders (account `0` exempt) and continues matching; default off.
- Replace: cancel-replace resting GTC by id (new price/qty; qty 0 cancels);
  loses time priority; pending stops are not replaceable.
- Mass cancel: filter resting orders / stops by account, symbol, and/or side.
- Fees: optional maker/taker bps on Engine fills (`Trade.maker_fee` /
  `taker_fee`, cumulative `fees_paid(account)`).
- Cash: Engine ledger in tick×qty; buys debit notional (+fee), sells credit
  (−fee). Optional `enforce_cash` rejects buys that exceed available cash
  (`cash - reserved`); resting GTC buys reserve `price * qty` until fill/cancel.
- Reject audit: JSONL `reject` records `RiskDecision` + attempted order; replay
  ignores it (session/audit only).
- Instruments are isolated: orders and last-trade stops never cross symbols.
- Trade price is the maker (resting) price.
- Engine assigns monotonic `TradeId` on fills (`Trade.id`; starts at 1).
- Persistence: `jsonl::save_event_log_file` / `load_event_log_file` round-trip
  the event stream (including `tif` / `symbol` / `stop`); `replay(Engine&)` is
  deterministic. Bare `OrderBook` replay rejects stop events.

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

Multithreading, lock-free structures, networking, and databases in the matching
hot path. Correctness and measurement come first.
