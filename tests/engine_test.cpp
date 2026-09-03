#include "mercury/engine.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Engine;
using mercury::MarketOrder;
using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;

namespace {

Order make_limit(std::uint64_t id, Side side, Price price, std::uint64_t qty,
                 AccountId account) {
  return Order{
      .id = OrderId{id},
      .side = side,
      .price = price,
      .quantity = Quantity{qty},
      .account = account,
  };
}

}  // namespace

TEST(Engine, TradeUpdatesMakerAndTakerPositions) {
  Engine engine;
  const AccountId maker{1};
  const AccountId taker{2};

  engine.add(make_limit(1, Side::Sell, Price{100}, 5, maker));
  const auto trades = engine.add(make_limit(2, Side::Buy, Price{100}, 5, taker));

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].maker_account, maker);
  EXPECT_EQ(trades[0].taker_account, taker);

  EXPECT_EQ(engine.positions().quantity(maker), -5);
  EXPECT_EQ(engine.positions().quantity(taker), 5);
  EXPECT_FALSE(engine.book().best_ask().has_value());
}

TEST(Engine, MarketTradeUpdatesPositions) {
  Engine engine;
  engine.add(make_limit(1, Side::Buy, Price{120}, 4, AccountId{1}));

  const auto trades = engine.add_market(MarketOrder{
      .id = OrderId{2},
      .side = Side::Sell,
      .quantity = Quantity{4},
      .account = AccountId{2},
  });

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(engine.positions().quantity(AccountId{1}), 4);
  EXPECT_EQ(engine.positions().quantity(AccountId{2}), -4);
}

TEST(Engine, CancelDoesNotChangePositions) {
  Engine engine;
  engine.add(make_limit(1, Side::Buy, Price{100}, 5, AccountId{1}));

  EXPECT_TRUE(engine.cancel(OrderId{1}));
  EXPECT_EQ(engine.positions().quantity(AccountId{1}), 0);
  EXPECT_FALSE(engine.book().best_bid().has_value());
}
