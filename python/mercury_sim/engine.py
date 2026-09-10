from __future__ import annotations

from dataclasses import dataclass
from typing import Literal, Optional

from mercury_sim.book import Order, OrderBook, Trade
from mercury_sim.events import Event, LimitEvent, MarketEvent, MassCancelEvent, ReplaceEvent, StopEvent


@dataclass
class _PendingStop:
    event: StopEvent


class Engine:
    """Python twin of C++ Engine: per-symbol books + last-trade stop triggers."""

    def __init__(
        self,
        stp: Literal["off", "cancel_resting"] = "off",
        maker_bps: int = 0,
        taker_bps: int = 0,
    ) -> None:
        self._stp = stp
        self._maker_bps = maker_bps
        self._taker_bps = taker_bps
        self._books: dict[int, OrderBook] = {}
        self._stops: dict[int, list[_PendingStop]] = {}
        self._last_trade: dict[int, int] = {}
        self._stop_index: dict[int, int] = {}  # order id -> symbol
        self._fees_paid: dict[int, int] = {}
        self._positions: dict[tuple[int, int], int] = {}
        self._avg_ticks: dict[tuple[int, int], int] = {}
        self._realized: dict[tuple[int, int], int] = {}

    def book(self, symbol: int = 0) -> OrderBook:
        return self._books.setdefault(symbol, OrderBook(stp=self._stp))

    def position(self, account: int, symbol: int = 0) -> int:
        return self._positions.get((account, symbol), 0)

    def realized_pnl(self, account: int, symbol: int = 0) -> int:
        return self._realized.get((account, symbol), 0)

    def unrealized_pnl(self, account: int, symbol: int = 0) -> Optional[int]:
        mark = self._last_trade.get(symbol)
        if mark is None:
            return None
        qty = self.position(account, symbol)
        if qty == 0:
            return 0
        return (mark - self._avg_ticks.get((account, symbol), 0)) * qty

    def fees_paid(self, account: int) -> int:
        return self._fees_paid.get(account, 0)

    def last_trade_price(self, symbol: int = 0) -> Optional[int]:
        return self._last_trade.get(symbol)

    def pending_stop_count(self, symbol: int = 0) -> int:
        return len(self._stops.get(symbol, []))

    def snapshot(self, max_levels: int, symbol: int = 0):
        return self.book(symbol).snapshot(max_levels)

    def add_limit(self, event: LimitEvent) -> list[Trade]:
        if event.reduce_only and not self._allows_reduce_only(
            event.account, event.side, event.quantity, event.symbol
        ):
            return []
        order = Order(
            id=event.id,
            side=event.side,
            price=event.price,
            quantity=event.quantity,
            account=event.account,
            tif=event.tif,
            symbol=event.symbol,
            post_only=event.post_only,
            reduce_only=event.reduce_only,
        )
        trades = self.book(event.symbol).add_limit(order)
        self._note_trades(event.symbol, trades, event.side)
        trades.extend(self._drain_stops(event.symbol))
        return trades

    def add_market(self, event: MarketEvent) -> list[Trade]:
        if event.reduce_only and not self._allows_reduce_only(
            event.account, event.side, event.quantity, event.symbol
        ):
            return []
        order = Order(
            id=event.id,
            side=event.side,
            price=0,
            quantity=event.quantity,
            account=event.account,
            symbol=event.symbol,
            reduce_only=event.reduce_only,
        )
        trades = self.book(event.symbol).add_market(order)
        self._note_trades(event.symbol, trades, event.side)
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

    def replace(self, order_id: int, price: int, quantity: int) -> Optional[list[Trade]]:
        if order_id in self._stop_index:
            return None
        for symbol, book in self._books.items():
            original = next((o for o in book.live_orders() if o.id == order_id), None)
            trades = book.replace(order_id, price, quantity)
            if trades is None:
                continue
            side = original.side if original is not None else "buy"
            self._note_trades(symbol, trades, side)
            trades.extend(self._drain_stops(symbol))
            return trades
        return None

    def mass_cancel(
        self,
        account: Optional[int] = None,
        symbol: Optional[int] = None,
        side: Optional[Literal["buy", "sell"]] = None,
    ) -> int:
        ids: list[int] = []
        for sym, pending in self._stops.items():
            if symbol is not None and sym != symbol:
                continue
            for item in pending:
                event = item.event
                if account is not None and event.account != account:
                    continue
                if side is not None and event.side != side:
                    continue
                ids.append(event.id)
        for sym, book in self._books.items():
            if symbol is not None and sym != symbol:
                continue
            for order in book.live_orders():
                if account is not None and order.account != account:
                    continue
                if side is not None and order.side != side:
                    continue
                ids.append(order.id)

        cancelled = 0
        for order_id in dict.fromkeys(ids):
            if self.cancel(order_id):
                cancelled += 1
        return cancelled

    def apply(self, event: Event) -> list[Trade]:
        if isinstance(event, LimitEvent):
            return self.add_limit(event)
        if isinstance(event, MarketEvent):
            return self.add_market(event)
        if isinstance(event, StopEvent):
            return self.add_stop(event)
        if isinstance(event, ReplaceEvent):
            trades = self.replace(event.id, event.price, event.quantity)
            return trades if trades is not None else []
        if isinstance(event, MassCancelEvent):
            self.mass_cancel(account=event.account, symbol=event.symbol, side=event.side)
            return []
        self.cancel(event.id)
        return []

    def _allows_reduce_only(
        self, account: int, side: Literal["buy", "sell"], quantity: int, symbol: int
    ) -> bool:
        pos = self.position(account, symbol)
        if side == "buy":
            return pos < 0 and quantity <= -pos
        return pos > 0 and quantity <= pos

    def _note_trades(
        self, symbol: int, trades: list[Trade], taker_side: Literal["buy", "sell"]
    ) -> None:
        for trade in trades:
            notional = trade.price * trade.quantity
            maker_fee = (notional * self._maker_bps) // 10_000
            taker_fee = (notional * self._taker_bps) // 10_000
            self._fees_paid[trade.maker_account] = (
                self._fees_paid.get(trade.maker_account, 0) + maker_fee
            )
            self._fees_paid[trade.taker_account] = (
                self._fees_paid.get(trade.taker_account, 0) + taker_fee
            )
            delta = trade.quantity if taker_side == "buy" else -trade.quantity
            self._apply_fill(trade.taker_account, symbol, delta, trade.price)
            self._apply_fill(trade.maker_account, symbol, -delta, trade.price)
        if trades:
            self._last_trade[symbol] = trades[-1].price

    def _apply_fill(self, account: int, symbol: int, delta: int, price: int) -> None:
        key = (account, symbol)
        qty = self._positions.get(key, 0)
        avg = self._avg_ticks.get(key, 0)
        realized = self._realized.get(key, 0)

        if qty == 0 or (qty > 0) == (delta > 0):
            abs_old = abs(qty)
            abs_add = abs(delta)
            self._avg_ticks[key] = (abs_old * avg + abs_add * price) // (abs_old + abs_add)
            self._positions[key] = qty + delta
            return

        close_qty = min(abs(delta), abs(qty))
        if qty > 0:
            realized += (price - avg) * close_qty
        else:
            realized += (avg - price) * close_qty

        previous = qty
        qty += delta
        self._realized[key] = realized
        self._positions[key] = qty
        if qty == 0:
            self._avg_ticks[key] = 0
        elif (previous > 0) != (qty > 0):
            self._avg_ticks[key] = price

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
