#include "mercury/engine.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Engine;
using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::RiskDecision;
using mercury::RiskLimits;
using mercury::Side;
using mercury::Symbol;

namespace {

Order make_limit(std::uint64_t id, Side side, Price price, std::uint64_t qty,
                 AccountId account, Symbol symbol) {
  return Order{.id = OrderId{id},
               .side = side,
               .price = price,
               .quantity = Quantity{qty},
               .account = account,
               .symbol = symbol};
}

}  // namespace

TEST(EngineMulti, BooksAreIsolatedBySymbol) {
  Engine engine;
  const Symbol a{1};
  const Symbol b{2};

  ASSERT_EQ(engine.add(make_limit(1, Side::Sell, Price{100}, 5, AccountId{1}, a)).decision,
            RiskDecision::Accept);
  ASSERT_EQ(engine.add(make_limit(2, Side::Sell, Price{200}, 7, AccountId{1}, b)).decision,
            RiskDecision::Accept);

  EXPECT_EQ(engine.book(a).best_ask(), Price{100});
  EXPECT_EQ(engine.book(b).best_ask(), Price{200});
  EXPECT_FALSE(engine.book(Symbol{0}).best_ask());

  const auto fill_a =
      engine.add(make_limit(3, Side::Buy, Price{100}, 5, AccountId{2}, a));
  ASSERT_EQ(fill_a.decision, RiskDecision::Accept);
  ASSERT_EQ(fill_a.trades.size(), 1u);
  EXPECT_EQ(fill_a.trades.front().symbol, a);

  EXPECT_FALSE(engine.book(a).best_ask());
  EXPECT_EQ(engine.book(b).best_ask(), Price{200});
  EXPECT_EQ(engine.positions().quantity(AccountId{2}, a), 5);
  EXPECT_EQ(engine.positions().quantity(AccountId{2}, b), 0);
}

TEST(EngineMulti, PositionsIndependentPerSymbol) {
  Engine engine;
  const Symbol a{1};
  const Symbol b{2};

  engine.add(make_limit(1, Side::Sell, Price{10}, 4, AccountId{1}, a));
  engine.add(make_limit(2, Side::Buy, Price{10}, 4, AccountId{2}, a));
  engine.add(make_limit(3, Side::Sell, Price{50}, 3, AccountId{1}, b));
  engine.add(make_limit(4, Side::Buy, Price{50}, 3, AccountId{2}, b));

  EXPECT_EQ(engine.positions().quantity(AccountId{2}, a), 4);
  EXPECT_EQ(engine.positions().quantity(AccountId{2}, b), 3);
  EXPECT_EQ(engine.positions().quantity(AccountId{1}, a), -4);
  EXPECT_EQ(engine.positions().quantity(AccountId{1}, b), -3);
}

TEST(EngineMulti, RiskLimitIsPerSymbol) {
  Engine engine{RiskLimits{.max_abs_position = 5}};
  const Symbol a{1};
  const Symbol b{2};

  ASSERT_EQ(engine.add(make_limit(1, Side::Buy, Price{100}, 5, AccountId{1}, a)).decision,
            RiskDecision::Accept);
  // Same account can open another 5 on a different symbol.
  ASSERT_EQ(engine.add(make_limit(2, Side::Buy, Price{100}, 5, AccountId{1}, b)).decision,
            RiskDecision::Accept);
  EXPECT_EQ(engine.add(make_limit(3, Side::Buy, Price{100}, 1, AccountId{1}, a)).decision,
            RiskDecision::PositionLimit);
}

TEST(EngineMulti, CancelRoutesToCorrectBook) {
  Engine engine;
  const Symbol a{1};
  const Symbol b{2};

  ASSERT_EQ(engine.add(make_limit(1, Side::Buy, Price{100}, 5, AccountId{1}, a)).decision,
            RiskDecision::Accept);
  ASSERT_EQ(engine.add(make_limit(2, Side::Buy, Price{200}, 5, AccountId{1}, b)).decision,
            RiskDecision::Accept);

  EXPECT_TRUE(engine.cancel(OrderId{1}));
  EXPECT_FALSE(engine.book(a).best_bid());
  EXPECT_EQ(engine.book(b).best_bid(), Price{200});
}

TEST(EngineMulti, SnapshotPerSymbol) {
  Engine engine;
  const Symbol a{1};

  engine.add(make_limit(1, Side::Buy, Price{99}, 2, AccountId{1}, a));
  engine.add(make_limit(2, Side::Sell, Price{101}, 3, AccountId{1}, a));

  const auto snap = engine.snapshot(5, a);
  ASSERT_EQ(snap.bids.size(), 1u);
  ASSERT_EQ(snap.asks.size(), 1u);
  EXPECT_EQ(snap.bids.front().price, Price{99});
  EXPECT_EQ(snap.asks.front().price, Price{101});
  EXPECT_TRUE(engine.snapshot(5, Symbol{9}).bids.empty());
}
