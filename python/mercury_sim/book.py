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


@dataclass(frozen=True)
class Trade:
    maker_id: int
    taker_id: int
    maker_account: int
    taker_account: int
    price: int
    quantity: int


class OrderBook:
    def __init__(self) -> None:
        self._bids: dict[int, deque[Order]] = {}
        self._asks: dict[int, deque[Order]] = {}
        self._index: dict[int, tuple[Literal["buy", "sell"], int]] = {}

    def add_limit(self, order: Order) -> list[Trade]:
        trades = self._match(order, is_market=False)
        if order.quantity > 0:
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
