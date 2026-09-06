#include "mercury/engine.hpp"
#include "mercury/event_log.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Engine;
using mercury::EventLog;
using mercury::Order;
using mercury::OrderBook;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::ReplaceOrder;
using mercury::RiskDecision;
using mercury::Side;
using mercury::apply;
using mercury::replay;

TEST(OrderBookReplace, ChangesPriceAndLosesPriority) {
  OrderBook book;
  book.add(Order{.id = OrderId{1}, .side = Side::Buy, .price = Price{100},
                 .quantity = Quantity{5}, .account = AccountId{1}});
  book.add(Order{.id = OrderId{2}, .side = Side::Buy, .price = Price{100},
                 .quantity = Quantity{5}, .account = AccountId{2}});

  const auto trades = book.replace(OrderId{1}, Price{100}, Quantity{5});
  ASSERT_TRUE(trades.has_value());
  EXPECT_TRUE(trades->empty());

  // Replaced id 1 is now behind id 2 at the same price.
  const auto fill =
      book.add(Order{.id = OrderId{3}, .side = Side::Sell, .price = Price{100},
                     .quantity = Quantity{5}, .account = AccountId{3}});
  ASSERT_EQ(fill.size(), 1u);
  EXPECT_EQ(fill[0].maker_id, OrderId{2});
}

TEST(OrderBookReplace, ZeroQuantityCancels) {
  OrderBook book;
  book.add(Order{.id = OrderId{1}, .side = Side::Sell, .price = Price{100},
                 .quantity = Quantity{3}});
  const auto trades = book.replace(OrderId{1}, Price{100}, Quantity{0});
  ASSERT_TRUE(trades.has_value());
  EXPECT_TRUE(trades->empty());
  EXPECT_FALSE(book.best_ask());
}

TEST(OrderBookReplace, UnknownId) {
  OrderBook book;
  EXPECT_FALSE(book.replace(OrderId{9}, Price{100}, Quantity{1}).has_value());
}

TEST(OrderBookReplace, CanCrossOnReentry) {
  OrderBook book;
  book.add(Order{.id = OrderId{1}, .side = Side::Buy, .price = Price{100},
                 .quantity = Quantity{5}, .account = AccountId{1}});
  book.add(Order{.id = OrderId{2}, .side = Side::Sell, .price = Price{110},
                 .quantity = Quantity{5}, .account = AccountId{2}});

  const auto trades = book.replace(OrderId{1}, Price{110}, Quantity{5});
  ASSERT_TRUE(trades.has_value());
  ASSERT_EQ(trades->size(), 1u);
  EXPECT_EQ((*trades)[0].price, Price{110});
  EXPECT_FALSE(book.best_bid());
  EXPECT_FALSE(book.best_ask());
}

TEST(EngineReplace, UpdatesWorkingExposure) {
  Engine engine;
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1}, .side = Side::Buy, .price = Price{100},
                           .quantity = Quantity{5}, .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);

  const auto replaced = engine.replace(OrderId{1}, Price{99}, Quantity{2});
  ASSERT_TRUE(replaced.has_value());
  EXPECT_EQ(replaced->decision, RiskDecision::Accept);
  EXPECT_EQ(engine.book().best_bid(), Price{99});

  EXPECT_TRUE(engine.cancel(OrderId{1}));
  EXPECT_FALSE(engine.book().best_bid());
}

TEST(EngineReplace, RejectsPendingStop) {
  Engine engine;
  ASSERT_EQ(engine
                .add_stop(mercury::StopOrder{.id = OrderId{1},
                                             .side = Side::Buy,
                                             .stop_price = Price{100},
                                             .quantity = Quantity{3},
                                             .account = AccountId{1},
                                             .limit_price = std::nullopt})
                .decision,
            RiskDecision::Accept);
  EXPECT_FALSE(engine.replace(OrderId{1}, Price{101}, Quantity{3}).has_value());
}

TEST(EventLog, ReplaceReplay) {
  EventLog log;
  log.append(Order{.id = OrderId{1}, .side = Side::Buy, .price = Price{100},
                   .quantity = Quantity{5}, .account = AccountId{1}});
  log.append(ReplaceOrder{.id = OrderId{1}, .price = Price{100}, .quantity = Quantity{2}});
  log.append(Order{.id = OrderId{2}, .side = Side::Sell, .price = Price{100},
                   .quantity = Quantity{2}, .account = AccountId{2}});

  Engine engine;
  const auto trades = replay(engine, log);
  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].quantity, Quantity{2});
  EXPECT_FALSE(engine.book().best_bid());
}
