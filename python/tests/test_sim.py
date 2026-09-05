import unittest

from mercury_sim.inventory import InventoryBook
from mercury_sim.sim import SimConfig, _skew_ticks, _taker_side, simulate_market
from mercury_sim.replay import replay


class InventoryTests(unittest.TestCase):
    def test_trade_updates_both_accounts(self) -> None:
        book = InventoryBook()
        book.apply_trade(maker_account=1, taker_account=2, taker_side="buy", quantity=4)
        self.assertEqual(book.quantity(2), 4)
        self.assertEqual(book.quantity(1), -4)


class SimTests(unittest.TestCase):
    def test_deterministic(self) -> None:
        cfg = SimConfig(n_events=80, seed=11)
        self.assertEqual(simulate_market(cfg), simulate_market(cfg))

    def test_event_budget(self) -> None:
        events = simulate_market(SimConfig(n_events=50, seed=3))
        self.assertEqual(len(events), 50)

    def test_replays_without_error(self) -> None:
        events = simulate_market(SimConfig(n_events=120, seed=5))
        trades, spreads = replay(events)
        self.assertGreaterEqual(len(trades), 0)
        self.assertEqual(len(spreads), len(events))

    def test_maker_skew_follows_inventory(self) -> None:
        cfg = SimConfig()
        self.assertGreater(_skew_ticks(3, cfg), 0)
        self.assertLess(_skew_ticks(-3, cfg), 0)
        self.assertEqual(_skew_ticks(0, cfg), 0)

    def test_taker_prefers_reducing_inventory(self) -> None:
        import random

        cfg = SimConfig(inventory_pressure=5)
        rng = random.Random(0)
        sells = sum(1 for _ in range(200) if _taker_side(rng, 10, cfg) == "sell")
        buys = sum(1 for _ in range(200) if _taker_side(rng, -10, cfg) == "buy")
        self.assertGreater(sells, 150)
        self.assertGreater(buys, 150)

    def test_different_seeds_differ(self) -> None:
        a = simulate_market(SimConfig(n_events=40, seed=1))
        b = simulate_market(SimConfig(n_events=40, seed=2))
        self.assertNotEqual(a, b)


if __name__ == "__main__":
    unittest.main()
