from __future__ import annotations

import random
from dataclasses import dataclass
from typing import Literal, Optional

from mercury_sim.book import Order, OrderBook, Trade
from mercury_sim.events import CancelEvent, Event, LimitEvent, MarketEvent
from mercury_sim.inventory import InventoryBook


@dataclass(frozen=True)
class SimConfig:
    n_events: int = 100
    seed: int = 1
    start_price: int = 10_000
    # Bernoulli arrival probability per discrete tick (empty ticks skipped).
    arrival_rate: float = 0.55
    # Probability an arrival is a market-maker action (else taker).
    maker_share: float = 0.45
    maker_account: int = 1
    taker_accounts: tuple[int, ...] = (2, 3)
    quote_size: int = 5
    taker_size: int = 3
    half_spread: int = 2
    inventory_skew_ticks: int = 1  # ticks shifted per inventory unit (capped)
    max_skew_ticks: int = 5
    inventory_pressure: int = 8  # |qty| above this strongly biases taker side


@dataclass
class _MakerState:
    bid_id: Optional[int] = None
    ask_id: Optional[int] = None


def _apply_trades(
    inventory: InventoryBook,
    trades: list[Trade],
    taker_side: Literal["buy", "sell"],
) -> None:
    for trade in trades:
        inventory.apply_trade(
            maker_account=trade.maker_account,
            taker_account=trade.taker_account,
            taker_side=taker_side,
            quantity=trade.quantity,
        )


def _mid(book: OrderBook, fallback: int) -> int:
    bid = book.best_bid()
    ask = book.best_ask()
    if bid is not None and ask is not None:
        return (bid + ask) // 2
    if bid is not None:
        return bid
    if ask is not None:
        return ask
    return fallback


def _skew_ticks(inventory: int, cfg: SimConfig) -> int:
    raw = max(-cfg.max_skew_ticks, min(cfg.max_skew_ticks, inventory * cfg.inventory_skew_ticks))
    return raw


def _maker_action(
    rng: random.Random,
    book: OrderBook,
    inventory: InventoryBook,
    cfg: SimConfig,
    state: _MakerState,
    next_id: int,
    mid_hint: int,
) -> tuple[list[Event], int]:
    """Cancel prior quotes and post inventory-skewed bid/ask."""
    events: list[Event] = []
    account = cfg.maker_account

    for order_id in (state.bid_id, state.ask_id):
        if order_id is not None and book.cancel(order_id):
            events.append(CancelEvent(id=order_id))
    state.bid_id = None
    state.ask_id = None

    mid = _mid(book, mid_hint)
    skew = _skew_ticks(inventory.quantity(account), cfg)
    # Long inventory -> lean sell: raise bid less, lower ask (more aggressive offer).
    bid_px = mid - cfg.half_spread - skew
    ask_px = mid + cfg.half_spread - skew
    if ask_px <= bid_px:
        ask_px = bid_px + 1

    bid = LimitEvent(
        id=next_id,
        side="buy",
        price=bid_px,
        quantity=cfg.quote_size,
        account=account,
    )
    next_id += 1
    ask = LimitEvent(
        id=next_id,
        side="sell",
        price=ask_px,
        quantity=cfg.quote_size,
        account=account,
    )
    next_id += 1

    for event in (bid, ask):
        events.append(event)
        trades = book.add_limit(
            Order(
                id=event.id,
                side=event.side,
                price=event.price,
                quantity=event.quantity,
                account=event.account,
            )
        )
        _apply_trades(inventory, trades, event.side)
        # Resting residual keeps the id live for later cancel.
        if book.is_live(event.id):
            if event.side == "buy":
                state.bid_id = event.id
            else:
                state.ask_id = event.id

    return events, next_id


def _taker_side(rng: random.Random, inventory: int, cfg: SimConfig) -> Literal["buy", "sell"]:
    # Prefer reducing inventory when pressure is high.
    if inventory >= cfg.inventory_pressure:
        return "sell" if rng.random() < 0.85 else "buy"
    if inventory <= -cfg.inventory_pressure:
        return "buy" if rng.random() < 0.85 else "sell"
    if inventory > 0:
        return "sell" if rng.random() < 0.6 else "buy"
    if inventory < 0:
        return "buy" if rng.random() < 0.6 else "sell"
    return "buy" if rng.random() < 0.5 else "sell"


def _taker_action(
    rng: random.Random,
    book: OrderBook,
    inventory: InventoryBook,
    cfg: SimConfig,
    next_id: int,
    mid_hint: int,
) -> tuple[list[Event], int]:
    account = rng.choice(cfg.taker_accounts)
    side = _taker_side(rng, inventory.quantity(account), cfg)
    qty = rng.randint(1, cfg.taker_size)

    use_market = rng.random() < 0.55
    if use_market:
        event: Event = MarketEvent(id=next_id, side=side, quantity=qty, account=account)
        trades = book.add_market(
            Order(id=event.id, side=side, price=0, quantity=qty, account=account)
        )
        _apply_trades(inventory, trades, side)
        return [event], next_id + 1

    mid = _mid(book, mid_hint)
    # Cross the spread a bit when inventory pressure is high.
    aggressive = abs(inventory.quantity(account)) >= cfg.inventory_pressure
    if side == "buy":
        price = mid + (cfg.half_spread + 1 if aggressive else rng.randint(0, cfg.half_spread))
    else:
        price = mid - (cfg.half_spread + 1 if aggressive else rng.randint(0, cfg.half_spread))

    event = LimitEvent(id=next_id, side=side, price=price, quantity=qty, account=account)
    trades = book.add_limit(
        Order(id=event.id, side=side, price=price, quantity=qty, account=account)
    )
    _apply_trades(inventory, trades, side)
    return [event], next_id + 1


def simulate_market(cfg: SimConfig | None = None, **overrides: object) -> list[Event]:
    """Discrete-time arrivals with an inventory-aware maker and takers."""
    if cfg is None:
        cfg = SimConfig()
    if overrides:
        cfg = SimConfig(**{**cfg.__dict__, **overrides})

    rng = random.Random(cfg.seed)
    book = OrderBook()
    inventory = InventoryBook()
    maker = _MakerState()
    events: list[Event] = []
    next_id = 1
    mid_hint = cfg.start_price

    # Seed a thin book so early takers have something to hit.
    seed_bid = LimitEvent(
        id=next_id,
        side="buy",
        price=cfg.start_price - cfg.half_spread,
        quantity=cfg.quote_size,
        account=cfg.maker_account,
    )
    next_id += 1
    seed_ask = LimitEvent(
        id=next_id,
        side="sell",
        price=cfg.start_price + cfg.half_spread,
        quantity=cfg.quote_size,
        account=cfg.maker_account,
    )
    next_id += 1
    for seed in (seed_bid, seed_ask):
        events.append(seed)
        book.add_limit(
            Order(
                id=seed.id,
                side=seed.side,
                price=seed.price,
                quantity=seed.quantity,
                account=seed.account,
            )
        )
        if seed.side == "buy":
            maker.bid_id = seed.id
        else:
            maker.ask_id = seed.id

    while len(events) < cfg.n_events:
        if rng.random() > cfg.arrival_rate:
            continue

        if rng.random() < cfg.maker_share:
            batch, next_id = _maker_action(
                rng, book, inventory, cfg, maker, next_id, mid_hint
            )
        else:
            batch, next_id = _taker_action(
                rng, book, inventory, cfg, next_id, mid_hint
            )

        events.extend(batch)
        mid_hint = _mid(book, mid_hint)

    return events[: cfg.n_events]
