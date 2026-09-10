#include "mercury/engine.hpp"
#include "mercury/jsonl.hpp"
#include "mercury/risk.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Engine;
using mercury::MarketOrder;
using mercury::Order;
using mercury::OrderId;
using mercury::Positions;
using mercury::Price;
using mercury::Quantity;
using mercury::RiskDecision;
using mercury::Side;
using mercury::check_reduce_only;
using mercury::jsonl::format_event_line;
using mercury::jsonl::parse_event_line;

TEST(Risk, ReduceOnlyAllowsClosingShort) {
  Positions positions;
  positions.fill(AccountId{1}, Side::Sell, Price{100}, Quantity{5});

  EXPECT_EQ(check_reduce_only(positions, AccountId{1}, Side::Buy, Quantity{5}),
            RiskDecision::Accept);
  EXPECT_EQ(check_reduce_only(positions, AccountId{1}, Side::Buy, Quantity{6}),
            RiskDecision::ReduceOnly);
  EXPECT_EQ(check_reduce_only(positions, AccountId{1}, Side::Sell, Quantity{1}),
            RiskDecision::ReduceOnly);
}

TEST(EngineReduceOnly, ClosesLongAndRejectsFlat) {
  Engine engine;
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{3},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{2},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{3},
                           .account = AccountId{2}})
                .decision,
            RiskDecision::Accept);
  EXPECT_EQ(engine.positions().quantity(AccountId{1}), 3);

  ASSERT_EQ(engine
                .add(Order{.id = OrderId{3},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{3},
                           .account = AccountId{3}})
                .decision,
            RiskDecision::Accept);

  const auto close = engine.add_market(MarketOrder{.id = OrderId{4},
                                                   .side = Side::Sell,
                                                   .quantity = Quantity{3},
                                                   .account = AccountId{1},
                                                   .reduce_only = true});
  EXPECT_EQ(close.decision, RiskDecision::Accept);
  ASSERT_EQ(close.trades.size(), 1u);
  EXPECT_EQ(engine.positions().quantity(AccountId{1}), 0);

  const auto reopen = engine.add(Order{.id = OrderId{5},
                                       .side = Side::Buy,
                                       .price = Price{100},
                                       .quantity = Quantity{1},
                                       .account = AccountId{1},
                                       .reduce_only = true});
  EXPECT_EQ(reopen.decision, RiskDecision::ReduceOnly);
  EXPECT_TRUE(reopen.trades.empty());
}

TEST(Jsonl, ReduceOnlyRoundTrip) {
  const Order limit{.id = OrderId{9},
                    .side = Side::Sell,
                    .price = Price{40},
                    .quantity = Quantity{1},
                    .account = AccountId{1},
                    .reduce_only = true};
  const auto parsed_limit = parse_event_line(format_event_line(limit));
  ASSERT_TRUE(std::holds_alternative<Order>(parsed_limit));
  EXPECT_EQ(std::get<Order>(parsed_limit), limit);

  const MarketOrder market{.id = OrderId{10},
                           .side = Side::Buy,
                           .quantity = Quantity{2},
                           .account = AccountId{1},
                           .reduce_only = true};
  const auto parsed_market = parse_event_line(format_event_line(market));
  ASSERT_TRUE(std::holds_alternative<MarketOrder>(parsed_market));
  EXPECT_EQ(std::get<MarketOrder>(parsed_market), market);
}
