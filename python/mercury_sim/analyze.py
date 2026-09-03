from __future__ import annotations

import argparse
from collections import Counter

from mercury_sim.events import LimitEvent, MarketEvent, read_jsonl


def summarize(path: str) -> dict:
    events = read_jsonl(path)
    types = Counter(type(event).__name__ for event in events)
    sides = Counter()
    prices: list[int] = []

    for event in events:
        if isinstance(event, (LimitEvent, MarketEvent)):
            sides[event.side] += 1
        if isinstance(event, LimitEvent):
            prices.append(event.price)

    return {
        "events": len(events),
        "types": dict(types),
        "sides": dict(sides),
        "price_min": min(prices) if prices else None,
        "price_max": max(prices) if prices else None,
    }


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Summarize Mercury event JSONL")
    parser.add_argument("path")
    args = parser.parse_args(argv)

    summary = summarize(args.path)
    print(f"events: {summary['events']}")
    print(f"types:  {summary['types']}")
    print(f"sides:  {summary['sides']}")
    print(f"prices: {summary['price_min']} .. {summary['price_max']}")


if __name__ == "__main__":
    main()
