#include "mercury/engine.hpp"

#include <gtest/gtest.h>

#include <optional>

using mercury::AccountId;
using mercury::Engine;
using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::RiskDecision;
using mercury::RiskLimits;
using mercury::Side;
using mercury::StopOrder;
using mercury::TimeInForce;

TEST(EngineStop, ArmsUntilLastTradeCrosses) {
  Engine engine;

  ASSERT_EQ(engine.add(Order{.id = OrderId{1}, .side = Side::Sell, .price = Price{100},
                             .quantity = Quantity{8}, .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);

  ASSERT_EQ(engine
                .add_stop(StopOrder{.id = OrderId{2},
                                    .side = Side::Buy,
                                    .stop_price = Price{100},
                                    .quantity = Quantity{3},
                                    .account = AccountId{2},
                                    .limit_price = std::nullopt})
                .decision,
            RiskDecision::Accept);
  EXPECT_EQ(engine.pending_stop_count(), 1u);
  EXPECT_FALSE(engine.last_trade_price());

  const auto crossed =
      engine.add(Order{.id = OrderId{3}, .side = Side::Buy, .price = Price{100},
                       .quantity = Quantity{5}, .account = AccountId{3}});
  ASSERT_EQ(crossed.decision, RiskDecision::Accept);
  EXPECT_EQ(crossed.trades.size(), 2u);  // seed fill + stop market
  EXPECT_EQ(engine.pending_stop_count(), 0u);
  ASSERT_TRUE(engine.last_trade_price());
  EXPECT_EQ(*engine.last_trade_price(), Price{100});
  EXPECT_EQ(engine.positions().quantity(AccountId{2}), 3);
}

TEST(EngineStop, SellTriggersWhenPriceFalls) {
  Engine engine;

  // Establish last trade at 100, then clear the book.
  ASSERT_EQ(engine.add(Order{.id = OrderId{1}, .side = Side::Sell, .price = Price{100},
                             .quantity = Quantity{5}, .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  ASSERT_EQ(engine.add(Order{.id = OrderId{2}, .side = Side::Buy, .price = Price{100},
                             .quantity = Quantity{5}, .account = AccountId{2}})
                .decision,
            RiskDecision::Accept);
  ASSERT_TRUE(engine.last_trade_price());
  EXPECT_EQ(*engine.last_trade_price(), Price{100});

  ASSERT_EQ(engine
                .add_stop(StopOrder{.id = OrderId{3},
                                    .side = Side::Sell,
                                    .stop_price = Price{95},
                                    .quantity = Quantity{2},
                                    .account = AccountId{3},
                                    .limit_price = std::nullopt})
                .decision,
            RiskDecision::Accept);
  EXPECT_EQ(engine.pending_stop_count(), 1u);

  // Fresh bids at 90; crossing sell prints 90 and fires the stop into remainder.
  ASSERT_EQ(engine.add(Order{.id = OrderId{4}, .side = Side::Buy, .price = Price{90},
                             .quantity = Quantity{10}, .account = AccountId{4}})
                .decision,
            RiskDecision::Accept);
  ASSERT_EQ(engine.add(Order{.id = OrderId{5}, .side = Side::Sell, .price = Price{90},
                             .quantity = Quantity{3}, .account = AccountId{5}})
                .decision,
            RiskDecision::Accept);
  EXPECT_EQ(engine.pending_stop_count(), 0u);
  EXPECT_EQ(engine.positions().quantity(AccountId{3}), -2);
}

TEST(EngineStop, ImmediateTriggerWhenAlreadyCrossed) {
  Engine engine;

  ASSERT_EQ(engine.add(Order{.id = OrderId{1}, .side = Side::Sell, .price = Price{100},
                             .quantity = Quantity{10}, .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  ASSERT_EQ(engine.add(Order{.id = OrderId{2}, .side = Side::Buy, .price = Price{100},
                             .quantity = Quantity{4}, .account = AccountId{2}})
                .decision,
            RiskDecision::Accept);

  const auto fired = engine.add_stop(StopOrder{.id = OrderId{3},
                                               .side = Side::Buy,
                                               .stop_price = Price{100},
                                               .quantity = Quantity{3},
                                               .account = AccountId{3},
                                               .limit_price = std::nullopt});
  ASSERT_EQ(fired.decision, RiskDecision::Accept);
  EXPECT_EQ(fired.trades.size(), 1u);
  EXPECT_EQ(engine.pending_stop_count(), 0u);
}

TEST(EngineStop, StopLimitRestsAfterTrigger) {
  Engine engine;

  ASSERT_EQ(engine.add(Order{.id = OrderId{1}, .side = Side::Sell, .price = Price{100},
                             .quantity = Quantity{2}, .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  ASSERT_EQ(engine.add(Order{.id = OrderId{2}, .side = Side::Buy, .price = Price{100},
                             .quantity = Quantity{2}, .account = AccountId{2}})
                .decision,
            RiskDecision::Accept);

  ASSERT_EQ(engine
                .add_stop(StopOrder{.id = OrderId{3},
                                    .side = Side::Buy,
                                    .stop_price = Price{100},
                                    .quantity = Quantity{5},
                                    .account = AccountId{3},
                                    .limit_price = Price{101}})
                .decision,
            RiskDecision::Accept);

  // No ask left at/below 101 — stop-limit rests.
  EXPECT_EQ(engine.pending_stop_count(), 0u);
  EXPECT_FALSE(engine.book().best_ask());
  ASSERT_TRUE(engine.book().best_bid());
  EXPECT_EQ(*engine.book().best_bid(), Price{101});
}

TEST(EngineStop, CancelPendingStop) {
  Engine engine;

  ASSERT_EQ(engine
                .add_stop(StopOrder{.id = OrderId{1},
                                    .side = Side::Buy,
                                    .stop_price = Price{100},
                                    .quantity = Quantity{5},
                                    .account = AccountId{1},
                                    .limit_price = std::nullopt})
                .decision,
            RiskDecision::Accept);
  EXPECT_TRUE(engine.cancel(OrderId{1}));
  EXPECT_EQ(engine.pending_stop_count(), 0u);
  EXPECT_FALSE(engine.cancel(OrderId{1}));
}

TEST(EngineStop, RiskRejectsStop) {
  Engine engine{RiskLimits{.max_order_quantity = Quantity{10}}};

  const auto rejected = engine.add_stop(StopOrder{.id = OrderId{1},
                                                  .side = Side::Buy,
                                                  .stop_price = Price{100},
                                                  .quantity = Quantity{11},
                                                  .account = AccountId{1},
                                                  .limit_price = std::nullopt});
  EXPECT_EQ(rejected.decision, RiskDecision::OrderTooLarge);
  EXPECT_EQ(engine.pending_stop_count(), 0u);
}

TEST(EngineStop, StopLimitIocDoesNotRest) {
  Engine engine;

  ASSERT_EQ(engine.add(Order{.id = OrderId{1}, .side = Side::Sell, .price = Price{100},
                             .quantity = Quantity{1}, .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  ASSERT_EQ(engine.add(Order{.id = OrderId{2}, .side = Side::Buy, .price = Price{100},
                             .quantity = Quantity{1}, .account = AccountId{2}})
                .decision,
            RiskDecision::Accept);

  ASSERT_EQ(engine
                .add_stop(StopOrder{.id = OrderId{3},
                                    .side = Side::Buy,
                                    .stop_price = Price{100},
                                    .quantity = Quantity{5},
                                    .account = AccountId{3},
                                    .limit_price = Price{105},
                                    .tif = TimeInForce::Ioc})
                .decision,
            RiskDecision::Accept);
  EXPECT_FALSE(engine.book().best_bid());
  EXPECT_FALSE(engine.book().best_ask());
}
