from __future__ import annotations

import argparse
from typing import Iterable, Optional

from mercury_sim.book import Order, OrderBook, Trade
from mercury_sim.engine import Engine
from mercury_sim.events import (
    Event,
    LimitEvent,
    MarketEvent,
    StopEvent,
    read_jsonl,
)


def apply_event(book: OrderBook, event: Event) -> list[Trade]:
    """Apply to a bare OrderBook. Stop events are not supported here."""
    if isinstance(event, StopEvent):
        raise ValueError("stop events require Engine replay")
    if isinstance(event, LimitEvent):
        return book.add_limit(
            Order(
                id=event.id,
                side=event.side,
                price=event.price,
                quantity=event.quantity,
                account=event.account,
                tif=event.tif,
                symbol=event.symbol,
            )
        )
    if isinstance(event, MarketEvent):
        return book.add_market(
            Order(
                id=event.id,
                side=event.side,
                price=0,
                quantity=event.quantity,
                account=event.account,
                symbol=event.symbol,
            )
        )
    book.cancel(event.id)
    return []


def replay(events: Iterable[Event]) -> tuple[list[Trade], list[Optional[int]]]:
    """Replay through Engine (supports stops, tif, multi-symbol)."""
    engine = Engine()
    trades: list[Trade] = []
    spreads: list[Optional[int]] = []
    for event in events:
        trades.extend(engine.apply(event))
        symbol = getattr(event, "symbol", 0)
        spreads.append(engine.book(symbol).spread())
    return trades, spreads


def summarize_trades(trades: list[Trade], spreads: list[Optional[int]]) -> dict:
    volume = sum(trade.quantity for trade in trades)
    notional = sum(trade.price * trade.quantity for trade in trades)
    observed = [spread for spread in spreads if spread is not None]
    return {
        "trades": len(trades),
        "volume": volume,
        "vwap": (notional / volume) if volume else None,
        "spread_samples": len(observed),
        "spread_avg": (sum(observed) / len(observed)) if observed else None,
        "best_spread_min": min(observed) if observed else None,
    }


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Replay Mercury events through a Python book")
    parser.add_argument("path")
    args = parser.parse_args(argv)

    trades, spreads = replay(read_jsonl(args.path))
    summary = summarize_trades(trades, spreads)
    print(f"trades:  {summary['trades']}")
    print(f"volume:  {summary['volume']}")
    print(f"vwap:    {summary['vwap']}")
    print(f"spread:  avg={summary['spread_avg']} min={summary['best_spread_min']}")


if __name__ == "__main__":
    main()
