#include "mercury/engine.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Engine;
using mercury::FeeSchedule;
using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::RiskDecision;
using mercury::RiskLimits;
using mercury::Side;

TEST(EngineCash, UpdatesOnFill) {
  Engine engine{{}, {}, FeeSchedule{.maker_bps = 0, .taker_bps = 0}};
  engine.set_cash(AccountId{2}, 10'000);

  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{5},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{2},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{5},
                           .account = AccountId{2}})
                .decision,
            RiskDecision::Accept);

  // notional = 100 * 5 = 500
  EXPECT_EQ(engine.cash(AccountId{2}), 10'000 - 500);
  EXPECT_EQ(engine.cash(AccountId{1}), 500);
}

TEST(EngineCash, RejectsBuyWhenEnforced) {
  Engine engine{RiskLimits{}, {}, FeeSchedule{}, true};
  engine.set_cash(AccountId{2}, 100);

  engine.add(Order{.id = OrderId{1},
                   .side = Side::Sell,
                   .price = Price{100},
                   .quantity = Quantity{5},
                   .account = AccountId{1}});
  const auto rejected = engine.add(Order{.id = OrderId{2},
                                         .side = Side::Buy,
                                         .price = Price{100},
                                         .quantity = Quantity{5},
                                         .account = AccountId{2}});
  EXPECT_EQ(rejected.decision, RiskDecision::InsufficientCash);
  EXPECT_TRUE(rejected.trades.empty());
  EXPECT_EQ(engine.cash(AccountId{2}), 100);
  EXPECT_EQ(engine.book().best_ask(), Price{100});
}

TEST(EngineCash, AcceptsBuyWithEnoughCash) {
  Engine engine;
  engine.set_enforce_cash(true);
  engine.set_cash(AccountId{2}, 500);

  engine.add(Order{.id = OrderId{1},
                   .side = Side::Sell,
                   .price = Price{100},
                   .quantity = Quantity{5},
                   .account = AccountId{1}});
  const auto fill = engine.add(Order{.id = OrderId{2},
                                     .side = Side::Buy,
                                     .price = Price{100},
                                     .quantity = Quantity{5},
                                     .account = AccountId{2}});
  EXPECT_EQ(fill.decision, RiskDecision::Accept);
  EXPECT_EQ(engine.cash(AccountId{2}), 0);
  EXPECT_EQ(engine.cash(AccountId{1}), 500);
}

TEST(EngineCash, ReservesRestingBuyAndReleasesOnCancel) {
  Engine engine;
  engine.set_enforce_cash(true);
  engine.set_cash(AccountId{1}, 1000);

  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{5},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  EXPECT_EQ(engine.cash(AccountId{1}), 1000);
  EXPECT_EQ(engine.reserved_cash(AccountId{1}), 500);
  EXPECT_EQ(engine.available_cash(AccountId{1}), 500);

  // Another 600-notional buy should fail while 500 is reserved.
  EXPECT_EQ(engine
                .add(Order{.id = OrderId{2},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{6},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::InsufficientCash);

  ASSERT_TRUE(engine.cancel(OrderId{1}));
  EXPECT_EQ(engine.reserved_cash(AccountId{1}), 0);
  EXPECT_EQ(engine.available_cash(AccountId{1}), 1000);
}

TEST(EngineCash, MakerBuyFillReleasesReservation) {
  Engine engine;
  engine.set_enforce_cash(true);
  engine.set_cash(AccountId{1}, 500);

  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{5},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  EXPECT_EQ(engine.reserved_cash(AccountId{1}), 500);

  ASSERT_EQ(engine
                .add(Order{.id = OrderId{2},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{5},
                           .account = AccountId{2}})
                .decision,
            RiskDecision::Accept);
  EXPECT_EQ(engine.cash(AccountId{1}), 0);
  EXPECT_EQ(engine.reserved_cash(AccountId{1}), 0);
  EXPECT_EQ(engine.available_cash(AccountId{1}), 0);
}
