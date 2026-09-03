import unittest

from mercury_sim.book import Order, OrderBook
from mercury_sim.generate import generate_events
from mercury_sim.replay import replay, summarize_trades


class BookTests(unittest.TestCase):
    def test_full_fill_at_maker_price(self) -> None:
        book = OrderBook()
        book.add_limit(Order(id=1, side="sell", price=100, quantity=5, account=1))
        trades = book.add_limit(Order(id=2, side="buy", price=120, quantity=5, account=2))

        self.assertEqual(len(trades), 1)
        self.assertEqual(trades[0].price, 100)
        self.assertEqual(trades[0].quantity, 5)
        self.assertIsNone(book.best_bid())
        self.assertIsNone(book.best_ask())

    def test_time_priority(self) -> None:
        book = OrderBook()
        book.add_limit(Order(id=1, side="sell", price=100, quantity=5))
        book.add_limit(Order(id=2, side="sell", price=100, quantity=5))
        trades = book.add_limit(Order(id=3, side="buy", price=100, quantity=5))

        self.assertEqual(trades[0].maker_id, 1)
        self.assertEqual(book.best_ask(), 100)

    def test_cancel(self) -> None:
        book = OrderBook()
        book.add_limit(Order(id=1, side="buy", price=100, quantity=5))
        self.assertTrue(book.cancel(1))
        self.assertFalse(book.cancel(1))
        self.assertIsNone(book.best_bid())

    def test_market_discards_remainder(self) -> None:
        book = OrderBook()
        book.add_limit(Order(id=1, side="sell", price=100, quantity=2))
        trades = book.add_market(Order(id=2, side="buy", price=0, quantity=5))

        self.assertEqual(trades[0].quantity, 2)
        self.assertIsNone(book.best_bid())
        self.assertIsNone(book.best_ask())

    def test_replay_is_deterministic(self) -> None:
        events = generate_events(80, seed=3)
        first, spreads_a = replay(events)
        second, spreads_b = replay(events)
        self.assertEqual(first, second)
        self.assertEqual(spreads_a, spreads_b)
        summary = summarize_trades(first, spreads_a)
        self.assertGreaterEqual(summary["trades"], 0)


if __name__ == "__main__":
    unittest.main()
