import tempfile
import unittest
from pathlib import Path

from mercury_sim.book import Order, OrderBook
from mercury_sim.engine import Engine
from mercury_sim.events import LimitEvent, StopEvent, event_from_dict, event_to_dict, write_jsonl
from mercury_sim.replay import apply_event, replay


class TifTests(unittest.TestCase):
    def test_ioc_discards_remainder(self) -> None:
        book = OrderBook()
        book.add_limit(Order(id=1, side="sell", price=100, quantity=2))
        trades = book.add_limit(
            Order(id=2, side="buy", price=100, quantity=5, tif="ioc")
        )
        self.assertEqual(trades[0].quantity, 2)
        self.assertIsNone(book.best_bid())
        self.assertIsNone(book.best_ask())

    def test_fok_rejects_partial(self) -> None:
        book = OrderBook()
        book.add_limit(Order(id=1, side="sell", price=100, quantity=2))
        trades = book.add_limit(
            Order(id=2, side="buy", price=100, quantity=5, tif="fok")
        )
        self.assertEqual(trades, [])
        self.assertEqual(book.best_ask(), 100)


class EngineStopTests(unittest.TestCase):
    def test_stop_fires_on_last_trade(self) -> None:
        engine = Engine()
        engine.apply(LimitEvent(id=1, side="sell", price=100, quantity=8, account=1))
        engine.apply(
            StopEvent(id=2, side="buy", stop_price=100, quantity=3, account=2)
        )
        self.assertEqual(engine.pending_stop_count(), 1)
        trades = engine.apply(
            LimitEvent(id=3, side="buy", price=100, quantity=5, account=3)
        )
        self.assertEqual(len(trades), 2)
        self.assertEqual(engine.pending_stop_count(), 0)
        self.assertEqual(engine.last_trade_price(), 100)

    def test_stop_roundtrip_dict(self) -> None:
        stop = StopEvent(
            id=9,
            side="sell",
            stop_price=95,
            quantity=2,
            account=4,
            limit_price=94,
            tif="ioc",
            symbol=2,
        )
        self.assertEqual(stop, event_from_dict(event_to_dict(stop)))

    def test_replay_stop_jsonl(self) -> None:
        events = [
            LimitEvent(id=1, side="sell", price=100, quantity=8, account=1),
            StopEvent(id=2, side="buy", stop_price=100, quantity=3, account=2),
            LimitEvent(id=3, side="buy", price=100, quantity=5, account=3),
        ]
        trades, _ = replay(events)
        self.assertEqual(len(trades), 2)

    def test_book_apply_rejects_stop(self) -> None:
        book = OrderBook()
        with self.assertRaises(ValueError):
            apply_event(
                book,
                StopEvent(id=1, side="buy", stop_price=100, quantity=1),
            )


class CppParityStopTests(unittest.TestCase):
    def test_stop_matches_cpp_when_available(self) -> None:
        from mercury_sim.compare import compare_files, find_jsonl_replay

        replay_bin = find_jsonl_replay()
        if replay_bin is None:
            self.skipTest("jsonl_replay not built")

        events = [
            LimitEvent(id=1, side="sell", price=100, quantity=8, account=1),
            StopEvent(id=2, side="buy", stop_price=100, quantity=3, account=2),
            LimitEvent(id=3, side="buy", price=100, quantity=5, account=3),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "stop.jsonl"
            write_jsonl(str(path), events)
            py_trades, cxx_trades = compare_files(path, replay_bin)
        self.assertEqual(py_trades, cxx_trades)


if __name__ == "__main__":
    unittest.main()
