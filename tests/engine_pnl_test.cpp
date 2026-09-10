#include "mercury/engine.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Engine;
using mercury::MarkSource;
using mercury::MarketOrder;
using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::RiskDecision;
using mercury::Side;
using mercury::Symbol;

TEST(EnginePnl, UnrealizedUsesLastTradeMark) {
  Engine engine;
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{5},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  EXPECT_FALSE(engine.unrealized_pnl(AccountId{1}).has_value());

  ASSERT_EQ(engine
                .add(Order{.id = OrderId{2},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{5},
                           .account = AccountId{2}})
                .decision,
            RiskDecision::Accept);

  EXPECT_EQ(engine.last_trade_price(), Price{100});
  ASSERT_TRUE(engine.unrealized_pnl(AccountId{1}).has_value());
  EXPECT_EQ(*engine.unrealized_pnl(AccountId{1}), 0);
  EXPECT_EQ(engine.realized_pnl(AccountId{1}), 0);

  // Mark moves via another trade at 110; account 1 still long 5 @ 100.
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{3},
                           .side = Side::Buy,
                           .price = Price{110},
                           .quantity = Quantity{1},
                           .account = AccountId{3}})
                .decision,
            RiskDecision::Accept);
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{4},
                           .side = Side::Sell,
                           .price = Price{110},
                           .quantity = Quantity{1},
                           .account = AccountId{4}})
                .decision,
            RiskDecision::Accept);

  EXPECT_EQ(engine.last_trade_price(), Price{110});
  EXPECT_EQ(*engine.unrealized_pnl(AccountId{1}), 50);  // (110-100)*5
}

TEST(EnginePnl, UnrealizedMidMark) {
  Engine engine;
  engine.add(Order{.id = OrderId{1},
                   .side = Side::Buy,
                   .price = Price{100},
                   .quantity = Quantity{5},
                   .account = AccountId{1}});
  engine.add(Order{.id = OrderId{2},
                   .side = Side::Sell,
                   .price = Price{100},
                   .quantity = Quantity{5},
                   .account = AccountId{2}});
  // Flat book after the fill — seed a two-sided quote.
  engine.add(Order{.id = OrderId{3},
                   .side = Side::Buy,
                   .price = Price{108},
                   .quantity = Quantity{1},
                   .account = AccountId{3}});
  engine.add(Order{.id = OrderId{4},
                   .side = Side::Sell,
                   .price = Price{112},
                   .quantity = Quantity{1},
                   .account = AccountId{4}});

  EXPECT_EQ(engine.mid_price(), Price{110});  // (108+112)/2
  EXPECT_EQ(engine.last_trade_price(), Price{100});
  EXPECT_EQ(*engine.unrealized_pnl(AccountId{1}, Symbol{0}, MarkSource::LastTrade), 0);
  EXPECT_EQ(*engine.unrealized_pnl(AccountId{1}, Symbol{0}, MarkSource::Mid), 50);
}

TEST(EnginePnl, RealizedOnClose) {
  Engine engine;
  engine.add(Order{.id = OrderId{1},
                   .side = Side::Buy,
                   .price = Price{100},
                   .quantity = Quantity{5},
                   .account = AccountId{1}});
  engine.add(Order{.id = OrderId{2},
                   .side = Side::Sell,
                   .price = Price{100},
                   .quantity = Quantity{5},
                   .account = AccountId{2}});
  engine.add(Order{.id = OrderId{3},
                   .side = Side::Buy,
                   .price = Price{110},
                   .quantity = Quantity{5},
                   .account = AccountId{3}});
  engine.add_market(MarketOrder{.id = OrderId{4},
                                .side = Side::Sell,
                                .quantity = Quantity{5},
                                .account = AccountId{1}});

  EXPECT_EQ(engine.positions().quantity(AccountId{1}), 0);
  EXPECT_EQ(engine.realized_pnl(AccountId{1}), 50);
  EXPECT_EQ(*engine.unrealized_pnl(AccountId{1}), 0);
}
