#include "mercury/engine.hpp"
#include "mercury/event_log.hpp"
#include "mercury/jsonl.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Engine;
using mercury::EventLog;
using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::RiskDecision;
using mercury::Side;
using mercury::TimeAdvance;
using mercury::TimeInForce;
using mercury::apply;
using mercury::jsonl::format_event_line;
using mercury::jsonl::parse_event_line;

TEST(EngineGtd, RejectsMissingOrPastExpire) {
  Engine engine;
  EXPECT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{1},
                           .tif = TimeInForce::Gtd,
                           .expire_at = 0})
                .decision,
            RiskDecision::InvalidExpire);

  engine.advance_time(10);
  EXPECT_EQ(engine
                .add(Order{.id = OrderId{2},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{1},
                           .tif = TimeInForce::Gtd,
                           .expire_at = 10})
                .decision,
            RiskDecision::InvalidExpire);
}

TEST(EngineGtd, AdvanceTimeCancelsExpired) {
  Engine engine;
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{1},
                           .account = AccountId{1},
                           .tif = TimeInForce::Gtd,
                           .expire_at = 5})
                .decision,
            RiskDecision::Accept);
  EXPECT_EQ(engine.book().best_bid(), Price{100});

  EXPECT_EQ(engine.advance_time(5), 1u);
  EXPECT_EQ(engine.now(), 5u);
  EXPECT_FALSE(engine.book().best_bid().has_value());
}

TEST(EngineGtd, TimeEventReplay) {
  EventLog log;
  log.append(Order{.id = OrderId{1},
                   .side = Side::Sell,
                   .price = Price{100},
                   .quantity = Quantity{1},
                   .account = AccountId{1},
                   .tif = TimeInForce::Gtd,
                   .expire_at = 3});
  log.append(TimeAdvance{.time = 3});

  Engine engine;
  apply(engine, log.at(0));
  EXPECT_EQ(engine.book().best_ask(), Price{100});
  apply(engine, log.at(1));
  EXPECT_FALSE(engine.book().best_ask().has_value());
}

TEST(Jsonl, GtdAndTimeRoundTrip) {
  const Order order{.id = OrderId{9},
                    .side = Side::Buy,
                    .price = Price{50},
                    .quantity = Quantity{2},
                    .tif = TimeInForce::Gtd,
                    .expire_at = 42};
  const auto parsed = parse_event_line(format_event_line(order));
  ASSERT_TRUE(std::holds_alternative<Order>(parsed));
  EXPECT_EQ(std::get<Order>(parsed), order);

  const TimeAdvance tick{.time = 7};
  const auto tick_parsed = parse_event_line(format_event_line(tick));
  ASSERT_TRUE(std::holds_alternative<TimeAdvance>(tick_parsed));
  EXPECT_EQ(std::get<TimeAdvance>(tick_parsed), tick);
}

TEST(EngineShortMargin, RejectsUncoveredSell) {
  Engine engine;
  engine.set_enforce_cash(true);
  engine.set_cash(AccountId{1}, 100);

  EXPECT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{5},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::InsufficientCash);
}

TEST(EngineShortMargin, CoveredSellNeedsNoMargin) {
  Engine engine;
  engine.set_enforce_cash(true);
  // Seed a long via unenforced path then enable checks.
  engine.set_enforce_cash(false);
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
  ASSERT_EQ(engine.positions().quantity(AccountId{1}), 5);

  engine.set_enforce_cash(true);
  engine.set_cash(AccountId{1}, 0);
  EXPECT_EQ(engine
                .add(Order{.id = OrderId{3},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{5},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
}

TEST(EngineShortMargin, ReservesShortPortionOfRestingSell) {
  Engine engine;
  engine.set_enforce_cash(true);
  engine.set_cash(AccountId{1}, 500);

  // Flat account: full sell qty is short -> reserve 100*3=300
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{3},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  EXPECT_EQ(engine.reserved_cash(AccountId{1}), 300);
  EXPECT_EQ(engine.available_cash(AccountId{1}), 200);

  ASSERT_TRUE(engine.cancel(OrderId{1}));
  EXPECT_EQ(engine.reserved_cash(AccountId{1}), 0);
  EXPECT_EQ(engine.available_cash(AccountId{1}), 500);
}

TEST(EngineShortMargin, PartialCoverReservesOnlyShortRest) {
  Engine engine;
  engine.set_enforce_cash(false);
  engine.add(Order{.id = OrderId{1},
                   .side = Side::Buy,
                   .price = Price{100},
                   .quantity = Quantity{7},
                   .account = AccountId{1}});
  engine.add(Order{.id = OrderId{2},
                   .side = Side::Sell,
                   .price = Price{100},
                   .quantity = Quantity{7},
                   .account = AccountId{2}});
  ASSERT_EQ(engine.positions().quantity(AccountId{1}), 7);

  engine.set_enforce_cash(true);
  engine.set_cash(AccountId{1}, 1000);
  // Sell 10 while long 7 -> short 3 reserved
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{3},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{10},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  EXPECT_EQ(engine.reserved_cash(AccountId{1}), 300);
  EXPECT_EQ(engine.book().best_ask(), Price{100});
}
