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
using mercury::RejectEvent;
using mercury::RiskDecision;
using mercury::Side;
using mercury::make_reject;
using mercury::replay;
using mercury::jsonl::format_event_line;
using mercury::jsonl::parse_event_line;

TEST(Jsonl, RejectRoundTrip) {
  const Order order{.id = OrderId{9},
                    .side = Side::Buy,
                    .price = Price{100},
                    .quantity = Quantity{5},
                    .account = AccountId{2},
                    .post_only = true};
  const RejectEvent event = make_reject(RiskDecision::PostOnly, order);
  const auto parsed = parse_event_line(format_event_line(event));
  ASSERT_TRUE(std::holds_alternative<RejectEvent>(parsed));
  EXPECT_EQ(std::get<RejectEvent>(parsed), event);
}

TEST(EventLog, RejectReplayIsNoOp) {
  EventLog log;
  log.append(Order{.id = OrderId{1},
                   .side = Side::Sell,
                   .price = Price{100},
                   .quantity = Quantity{1},
                   .account = AccountId{1}});
  log.append(make_reject(RiskDecision::InsufficientCash,
                         Order{.id = OrderId{2},
                               .side = Side::Buy,
                               .price = Price{100},
                               .quantity = Quantity{1},
                               .account = AccountId{2}}));

  Engine engine;
  const auto trades = replay(engine, log);
  EXPECT_TRUE(trades.empty());
  EXPECT_EQ(engine.book().best_ask(), Price{100});
  EXPECT_FALSE(engine.book().best_bid().has_value());
}
