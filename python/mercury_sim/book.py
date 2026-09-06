from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from typing import Literal, Optional


@dataclass
class Order:
    id: int
    side: Literal["buy", "sell"]
    price: int
    quantity: int
    account: int = 0
    tif: Literal["gtc", "ioc", "fok"] = "gtc"
    symbol: int = 0


@dataclass(frozen=True)
class Trade:
    maker_id: int
    taker_id: int
    maker_account: int
    taker_account: int
    price: int
    quantity: int

    def to_dict(self) -> dict:
        return {
            "maker_id": self.maker_id,
            "taker_id": self.taker_id,
            "maker_account": self.maker_account,
            "taker_account": self.taker_account,
            "price": self.price,
            "quantity": self.quantity,
        }


@dataclass(frozen=True)
class BookLevel:
    price: int
    quantity: int
    order_count: int


@dataclass(frozen=True)
class BookSnapshot:
    bids: tuple[BookLevel, ...]  # best bid first
    asks: tuple[BookLevel, ...]  # best ask first

    def best_bid(self) -> Optional[int]:
        return self.bids[0].price if self.bids else None

    def best_ask(self) -> Optional[int]:
        return self.asks[0].price if self.asks else None

    def spread_ticks(self) -> Optional[int]:
        bid = self.best_bid()
        ask = self.best_ask()
        if bid is None or ask is None:
            return None
        return ask - bid


class OrderBook:
    def __init__(self) -> None:
        self._bids: dict[int, deque[Order]] = {}
        self._asks: dict[int, deque[Order]] = {}
        self._index: dict[int, tuple[Literal["buy", "sell"], int]] = {}

    def add_limit(self, order: Order) -> list[Trade]:
        if order.tif == "fok" and not self._can_fully_fill(order):
            return []

        trades = self._match(order, is_market=False)
        if order.tif == "gtc" and order.quantity > 0:
            self._rest(order)
        return trades

    def add_market(self, order: Order) -> list[Trade]:
        return self._match(order, is_market=True)

    def cancel(self, order_id: int) -> bool:
        loc = self._index.get(order_id)
        if loc is None:
            return False
        side, price = loc
        levels = self._bids if side == "buy" else self._asks
        queue = levels[price]
        for i, order in enumerate(queue):
            if order.id == order_id:
                del queue[i]
                break
        if not queue:
            del levels[price]
        del self._index[order_id]
        return True

    def is_live(self, order_id: int) -> bool:
        return order_id in self._index

    def best_bid(self) -> Optional[int]:
        return max(self._bids) if self._bids else None

    def best_ask(self) -> Optional[int]:
        return min(self._asks) if self._asks else None

    def spread(self) -> Optional[int]:
        bid = self.best_bid()
        ask = self.best_ask()
        if bid is None or ask is None:
            return None
        return ask - bid

    def snapshot(self, max_levels: int) -> BookSnapshot:
        bids = tuple(
            BookLevel(
                price=price,
                quantity=sum(order.quantity for order in self._bids[price]),
                order_count=len(self._bids[price]),
            )
            for price in sorted(self._bids, reverse=True)[:max_levels]
        )
        asks = tuple(
            BookLevel(
                price=price,
                quantity=sum(order.quantity for order in self._asks[price]),
                order_count=len(self._asks[price]),
            )
            for price in sorted(self._asks)[:max_levels]
        )
        return BookSnapshot(bids=bids, asks=asks)

    def _can_fully_fill(self, order: Order) -> bool:
        available = 0
        if order.side == "buy":
            for price in sorted(self._asks):
                if price > order.price:
                    break
                available += sum(o.quantity for o in self._asks[price])
                if available >= order.quantity:
                    return True
        else:
            for price in sorted(self._bids, reverse=True):
                if price < order.price:
                    break
                available += sum(o.quantity for o in self._bids[price])
                if available >= order.quantity:
                    return True
        return False

    def _rest(self, order: Order) -> None:
        levels = self._bids if order.side == "buy" else self._asks
        levels.setdefault(order.price, deque()).append(order)
        self._index[order.id] = (order.side, order.price)

    def _match(self, taker: Order, is_market: bool) -> list[Trade]:
        trades: list[Trade] = []
        if taker.side == "buy":
            while taker.quantity > 0 and self._asks:
                best = min(self._asks)
                if not is_market and best > taker.price:
                    break
                self._fill_level(self._asks, best, taker, trades)
        else:
            while taker.quantity > 0 and self._bids:
                best = max(self._bids)
                if not is_market and best < taker.price:
                    break
                self._fill_level(self._bids, best, taker, trades)
        return trades

    def _fill_level(
        self,
        levels: dict[int, deque[Order]],
        price: int,
        taker: Order,
        trades: list[Trade],
    ) -> None:
        queue = levels[price]
        while taker.quantity > 0 and queue:
            maker = queue[0]
            fill = min(taker.quantity, maker.quantity)
            trades.append(
                Trade(
                    maker_id=maker.id,
                    taker_id=taker.id,
                    maker_account=maker.account,
                    taker_account=taker.account,
                    price=maker.price,
                    quantity=fill,
                )
            )
            taker.quantity -= fill
            maker.quantity -= fill
            if maker.quantity == 0:
                queue.popleft()
                del self._index[maker.id]
        if not queue:
            del levels[price]
