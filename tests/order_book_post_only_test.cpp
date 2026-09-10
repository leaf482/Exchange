#include "mercury/engine.hpp"
#include "mercury/jsonl.hpp"
#include "mercury/order_book.hpp"

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
using mercury::jsonl::format_event_line;
using mercury::jsonl::parse_event_line;

TEST(OrderBookPostOnly, RestsWhenNotCrossing) {
  OrderBook book;
  book.add(Order{.id = OrderId{1},
                 .side = Side::Sell,
                 .price = Price{110},
                 .quantity = Quantity{1}});

  const auto trades = book.add(Order{.id = OrderId{2},
                                     .side = Side::Buy,
                                     .price = Price{100},
                                     .quantity = Quantity{1},
                                     .post_only = true});

  EXPECT_TRUE(trades.empty());
  EXPECT_EQ(book.best_bid(), Price{100});
  EXPECT_EQ(book.best_ask(), Price{110});
}

TEST(OrderBookPostOnly, RejectsWhenWouldTake) {
  OrderBook book;
  book.add(Order{.id = OrderId{1},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{1}});

  const auto trades = book.add(Order{.id = OrderId{2},
                                     .side = Side::Buy,
                                     .price = Price{100},
                                     .quantity = Quantity{1},
                                     .post_only = true});

  EXPECT_TRUE(trades.empty());
  EXPECT_FALSE(book.best_bid().has_value());
  EXPECT_EQ(book.best_ask(), Price{100});
}

TEST(EnginePostOnly, RejectsCrossingWithoutFill) {
  Engine engine;
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{1},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);

  const auto rejected = engine.add(Order{.id = OrderId{2},
                                         .side = Side::Buy,
                                         .price = Price{100},
                                         .quantity = Quantity{1},
                                         .account = AccountId{2},
                                         .post_only = true});
  EXPECT_EQ(rejected.decision, RiskDecision::PostOnly);
  EXPECT_TRUE(rejected.trades.empty());
  EXPECT_EQ(engine.book().best_ask(), Price{100});
  EXPECT_FALSE(engine.book().best_bid().has_value());
}

TEST(Jsonl, PostOnlyRoundTrip) {
  const Order order{.id = OrderId{3},
                    .side = Side::Buy,
                    .price = Price{50},
                    .quantity = Quantity{2},
                    .account = AccountId{1},
                    .post_only = true};
  const auto parsed = parse_event_line(format_event_line(order));
  ASSERT_TRUE(std::holds_alternative<Order>(parsed));
  EXPECT_EQ(std::get<Order>(parsed), order);
  EXPECT_NE(format_event_line(order).find("\"post_only\":true"), std::string::npos);
}
