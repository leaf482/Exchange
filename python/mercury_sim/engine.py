from __future__ import annotations

from dataclasses import dataclass
from typing import Literal, Optional

from mercury_sim.book import Order, OrderBook, Trade
from mercury_sim.events import Event, LimitEvent, MarketEvent, StopEvent


@dataclass
class _PendingStop:
    event: StopEvent


class Engine:
    """Python twin of C++ Engine: per-symbol books + last-trade stop triggers."""

    def __init__(self, stp: Literal["off", "cancel_resting"] = "off") -> None:
        self._stp = stp
        self._books: dict[int, OrderBook] = {}
        self._stops: dict[int, list[_PendingStop]] = {}
        self._last_trade: dict[int, int] = {}
        self._stop_index: dict[int, int] = {}  # order id -> symbol

    def book(self, symbol: int = 0) -> OrderBook:
        return self._books.setdefault(symbol, OrderBook(stp=self._stp))

    def last_trade_price(self, symbol: int = 0) -> Optional[int]:
        return self._last_trade.get(symbol)

    def pending_stop_count(self, symbol: int = 0) -> int:
        return len(self._stops.get(symbol, []))

    def snapshot(self, max_levels: int, symbol: int = 0):
        return self.book(symbol).snapshot(max_levels)

    def add_limit(self, event: LimitEvent) -> list[Trade]:
        order = Order(
            id=event.id,
            side=event.side,
            price=event.price,
            quantity=event.quantity,
            account=event.account,
            tif=event.tif,
            symbol=event.symbol,
        )
        trades = self.book(event.symbol).add_limit(order)
        self._note_trades(event.symbol, trades)
        trades.extend(self._drain_stops(event.symbol))
        return trades

    def add_market(self, event: MarketEvent) -> list[Trade]:
        order = Order(
            id=event.id,
            side=event.side,
            price=0,
            quantity=event.quantity,
            account=event.account,
            symbol=event.symbol,
        )
        trades = self.book(event.symbol).add_market(order)
        self._note_trades(event.symbol, trades)
        trades.extend(self._drain_stops(event.symbol))
        return trades

    def add_stop(self, event: StopEvent) -> list[Trade]:
        if self._is_triggered(event):
            return self._fire_stop(event)
        self._stops.setdefault(event.symbol, []).append(_PendingStop(event=event))
        self._stop_index[event.id] = event.symbol
        return []

    def cancel(self, order_id: int) -> bool:
        symbol = self._stop_index.pop(order_id, None)
        if symbol is not None:
            pending = self._stops.get(symbol, [])
            self._stops[symbol] = [item for item in pending if item.event.id != order_id]
            return True

        for book in self._books.values():
            if book.cancel(order_id):
                return True
        return False

    def apply(self, event: Event) -> list[Trade]:
        if isinstance(event, LimitEvent):
            return self.add_limit(event)
        if isinstance(event, MarketEvent):
            return self.add_market(event)
        if isinstance(event, StopEvent):
            return self.add_stop(event)
        self.cancel(event.id)
        return []

    def _note_trades(self, symbol: int, trades: list[Trade]) -> None:
        if trades:
            self._last_trade[symbol] = trades[-1].price

    def _is_triggered(self, event: StopEvent) -> bool:
        last = self._last_trade.get(event.symbol)
        if last is None:
            return False
        if event.side == "buy":
            return last >= event.stop_price
        return last <= event.stop_price

    def _fire_stop(self, event: StopEvent) -> list[Trade]:
        if event.limit_price is None:
            return self.add_market(
                MarketEvent(
                    id=event.id,
                    side=event.side,
                    quantity=event.quantity,
                    account=event.account,
                    symbol=event.symbol,
                )
            )
        return self.add_limit(
            LimitEvent(
                id=event.id,
                side=event.side,
                price=event.limit_price,
                quantity=event.quantity,
                account=event.account,
                tif=event.tif,
                symbol=event.symbol,
            )
        )

    def _drain_stops(self, symbol: int) -> list[Trade]:
        trades: list[Trade] = []
        progressed = True
        while progressed:
            progressed = False
            pending = self._stops.get(symbol, [])
            for index, item in enumerate(pending):
                if not self._is_triggered(item.event):
                    continue
                stop = pending.pop(index).event
                self._stops[symbol] = pending
                self._stop_index.pop(stop.id, None)
                trades.extend(self._fire_stop(stop))
                progressed = True
                break
        return trades
