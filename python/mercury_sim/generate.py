from __future__ import annotations

import argparse
import random

from mercury_sim.events import CancelEvent, Event, LimitEvent, MarketEvent, write_jsonl


def generate_events(count: int, seed: int = 1, start_price: int = 10_000) -> list[Event]:
    rng = random.Random(seed)
    events: list[Event] = []
    next_id = 1
    mid = start_price
    live: list[int] = []

    for _ in range(count):
        roll = rng.random()
        if live and roll < 0.15:
            order_id = live.pop(rng.randrange(len(live)))
            events.append(CancelEvent(id=order_id))
            continue

        side = "buy" if rng.random() < 0.5 else "sell"
        if roll < 0.25:
            events.append(
                MarketEvent(
                    id=next_id,
                    side=side,
                    quantity=rng.randint(1, 5),
                    account=rng.randint(1, 3),
                )
            )
            next_id += 1
            continue

        mid += rng.choice([-1, 0, 0, 1])
        price = mid - rng.randint(0, 5) if side == "buy" else mid + rng.randint(0, 5)
        qty = rng.randint(1, 10)
        events.append(
            LimitEvent(
                id=next_id,
                side=side,
                price=price,
                quantity=qty,
                account=rng.randint(1, 3),
            )
        )
        live.append(next_id)
        next_id += 1

    return events


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Generate synthetic Mercury event JSONL")
    parser.add_argument("-n", "--count", type=int, default=100)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--start-price", type=int, default=10_000)
    parser.add_argument("-o", "--output", required=True)
    args = parser.parse_args(argv)

    events = generate_events(args.count, seed=args.seed, start_price=args.start_price)
    write_jsonl(args.output, events)
    print(f"wrote {len(events)} events to {args.output}")


if __name__ == "__main__":
    main()
