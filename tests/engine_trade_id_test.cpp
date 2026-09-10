#include "mercury/engine.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Engine;
using mercury::Order;
using mercury::OrderBook;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::RiskDecision;
using mercury::Side;
using mercury::TradeId;

TEST(EngineTradeId, AssignsMonotonicIds) {
  Engine engine;
  EXPECT_EQ(engine.next_trade_id(), TradeId{1});

  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{5},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  const auto first = engine.add(Order{.id = OrderId{2},
                                      .side = Side::Buy,
                                      .price = Price{100},
                                      .quantity = Quantity{2},
                                      .account = AccountId{2}});
  ASSERT_EQ(first.trades.size(), 1u);
  EXPECT_EQ(first.trades[0].id, TradeId{1});

  engine.add(Order{.id = OrderId{3},
                   .side = Side::Sell,
                   .price = Price{101},
                   .quantity = Quantity{1},
                   .account = AccountId{1}});
  const auto second = engine.add(Order{.id = OrderId{4},
                                       .side = Side::Buy,
                                       .price = Price{101},
                                       .quantity = Quantity{4},
                                       .account = AccountId{2}});
  ASSERT_EQ(second.trades.size(), 2u);
  EXPECT_EQ(second.trades[0].id, TradeId{2});
  EXPECT_EQ(second.trades[1].id, TradeId{3});
  EXPECT_EQ(engine.next_trade_id(), TradeId{4});
}

TEST(OrderBookTradeId, LeavesIdZero) {
  OrderBook book;
  book.add(Order{.id = OrderId{1},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{1}});
  const auto trades = book.add(Order{.id = OrderId{2},
                                     .side = Side::Buy,
                                     .price = Price{100},
                                     .quantity = Quantity{1}});
  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].id, TradeId{0});
}
