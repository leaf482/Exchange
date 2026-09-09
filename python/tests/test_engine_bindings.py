import unittest

try:
    import mercury_engine
except ImportError as exc:  # pragma: no cover - optional native module
    mercury_engine = None
    _IMPORT_ERROR = exc
else:
    _IMPORT_ERROR = None


@unittest.skipIf(mercury_engine is None, f"mercury_engine not built: {_IMPORT_ERROR}")
class EngineBindingsTests(unittest.TestCase):
    def test_limit_match_and_snapshot(self) -> None:
        engine = mercury_engine.Engine()
        sell = engine.add_limit(id=1, side=mercury_engine.Side.Sell, price=100, quantity=5, account=1)
        self.assertEqual(sell["decision"], mercury_engine.RiskDecision.Accept)
        self.assertEqual(sell["trades"], [])

        buy = engine.add_limit(id=2, side=mercury_engine.Side.Buy, price=100, quantity=5, account=2)
        self.assertEqual(len(buy["trades"]), 1)
        self.assertEqual(buy["trades"][0]["price"], 100)
        self.assertEqual(buy["trades"][0]["quantity"], 5)

        snap = engine.snapshot(5)
        self.assertEqual(snap["bids"], [])
        self.assertEqual(snap["asks"], [])
        self.assertIsNone(snap["best_bid"])
        self.assertEqual(engine.position(2), 5)
        self.assertEqual(engine.position(1), -5)
        self.assertEqual(engine.last_trade_price(), 100)

    def test_risk_limits(self) -> None:
        engine = mercury_engine.Engine(mercury_engine.RiskLimits(max_order_quantity=3))
        rejected = engine.add_limit(id=1, side=mercury_engine.Side.Buy, price=100, quantity=4)
        self.assertEqual(rejected["decision"], mercury_engine.RiskDecision.OrderTooLarge)

    def test_cancel(self) -> None:
        engine = mercury_engine.Engine()
        engine.add_limit(id=1, side=mercury_engine.Side.Buy, price=99, quantity=2)
        self.assertTrue(engine.cancel(1))
        self.assertFalse(engine.cancel(1))
        self.assertIsNone(engine.snapshot()["best_bid"])

    def test_replace_fees_mass_cancel(self) -> None:
        fees = mercury_engine.FeeSchedule(maker_bps=-10, taker_bps=20)
        engine = mercury_engine.Engine(
            limits=mercury_engine.RiskLimits(),
            stp=mercury_engine.SelfTradePrevention.Off,
            fees=fees,
        )
        engine.add_limit(id=1, side=mercury_engine.Side.Sell, price=1000, quantity=10, account=1)
        fill = engine.add_limit(id=2, side=mercury_engine.Side.Buy, price=1000, quantity=10, account=2)
        self.assertEqual(fill["trades"][0]["maker_fee"], -10)
        self.assertEqual(engine.fees_paid(1), -10)

        engine.add_limit(id=3, side=mercury_engine.Side.Buy, price=90, quantity=1, account=3)
        engine.add_limit(id=4, side=mercury_engine.Side.Buy, price=89, quantity=1, account=4)
        self.assertEqual(engine.mass_cancel(account=3), 1)
        replaced = engine.replace(4, 88, 1)
        self.assertIsNotNone(replaced)
        self.assertEqual(engine.snapshot()["best_bid"], 88)


if __name__ == "__main__":
    unittest.main()
