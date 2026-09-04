#include "mercury/jsonl.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <variant>

using mercury::Order;
using mercury::OrderBook;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;
using mercury::jsonl::load_event_log;
using mercury::jsonl::parse_event_line;
using mercury::replay;

TEST(Jsonl, ParsesLimitEvent) {
  const auto event = parse_event_line(
      R"({"id":1,"side":"buy","price":100,"quantity":5,"account":7,"type":"limit"})");

  ASSERT_TRUE(std::holds_alternative<Order>(event));
  const auto& order = std::get<Order>(event);
  EXPECT_EQ(order.id, OrderId{1});
  EXPECT_EQ(order.side, Side::Buy);
  EXPECT_EQ(order.price, Price{100});
  EXPECT_EQ(order.quantity, Quantity{5});
  EXPECT_EQ(order.account.value(), 7u);
}

TEST(Jsonl, ReplaysPythonStyleLog) {
  const char* text =
      R"({"id":1,"side":"sell","price":100,"quantity":5,"account":1,"type":"limit"})"
      "\n"
      R"({"id":2,"side":"buy","price":100,"quantity":5,"account":2,"type":"limit"})"
      "\n";

  const auto log = load_event_log(text);
  OrderBook book;
  const auto trades = replay(book, log);

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].maker_id, OrderId{1});
  EXPECT_EQ(trades[0].taker_id, OrderId{2});
  EXPECT_EQ(trades[0].price, Price{100});
  EXPECT_EQ(trades[0].quantity, Quantity{5});
  EXPECT_FALSE(book.best_bid().has_value());
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(Jsonl, CancelAndMarket) {
  const char* text =
      R"({"id":1,"side":"sell","price":100,"quantity":2,"account":1,"type":"limit"})"
      "\n"
      R"({"id":1,"type":"cancel"})"
      "\n"
      R"({"id":2,"side":"sell","price":110,"quantity":3,"account":1,"type":"limit"})"
      "\n"
      R"({"id":3,"side":"buy","quantity":5,"account":2,"type":"market"})"
      "\n";

  const auto log = load_event_log(text);
  OrderBook book;
  const auto trades = replay(book, log);

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].price, Price{110});
  EXPECT_EQ(trades[0].quantity, Quantity{3});
  EXPECT_FALSE(book.best_ask().has_value());
}
