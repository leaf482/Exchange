#include "mercury/jsonl.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <variant>

using mercury::AccountId;
using mercury::CancelOrder;
using mercury::EventLog;
using mercury::MarketOrder;
using mercury::Order;
using mercury::OrderBook;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;
using mercury::Symbol;
using mercury::TimeInForce;
using mercury::jsonl::format_event_line;
using mercury::jsonl::load_event_log;
using mercury::jsonl::load_event_log_file;
using mercury::jsonl::parse_event_line;
using mercury::jsonl::save_event_log_file;
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

TEST(Jsonl, ParsesTifAndSymbol) {
  const auto event = parse_event_line(
      R"({"type":"limit","id":1,"side":"buy","price":100,"quantity":5,"tif":"ioc","symbol":9})");

  ASSERT_TRUE(std::holds_alternative<Order>(event));
  const auto& order = std::get<Order>(event);
  EXPECT_EQ(order.tif, TimeInForce::Ioc);
  EXPECT_EQ(order.symbol, Symbol{9});
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

TEST(Jsonl, RoundTripFormatAndParse) {
  const Order order{.id = OrderId{1},
                    .side = Side::Buy,
                    .price = Price{100},
                    .quantity = Quantity{5},
                    .account = AccountId{7},
                    .tif = TimeInForce::Fok,
                    .symbol = Symbol{3}};
  const auto parsed = parse_event_line(format_event_line(order));
  ASSERT_TRUE(std::holds_alternative<Order>(parsed));
  EXPECT_EQ(std::get<Order>(parsed), order);
}

TEST(Jsonl, SaveLoadFileExactReplay) {
  EventLog original;
  original.append(Order{.id = OrderId{1},
                        .side = Side::Sell,
                        .price = Price{100},
                        .quantity = Quantity{5},
                        .account = AccountId{1},
                        .tif = TimeInForce::Gtc,
                        .symbol = Symbol{0}});
  original.append(Order{.id = OrderId{2},
                        .side = Side::Buy,
                        .price = Price{100},
                        .quantity = Quantity{8},
                        .account = AccountId{2},
                        .tif = TimeInForce::Ioc,
                        .symbol = Symbol{0}});
  original.append(MarketOrder{.id = OrderId{3},
                              .side = Side::Buy,
                              .quantity = Quantity{1},
                              .account = AccountId{3}});
  original.append(CancelOrder{.id = OrderId{2}});

  const auto path =
      std::filesystem::temp_directory_path() / "mercury_event_log_roundtrip.jsonl";
  save_event_log_file(path, original);
  const EventLog loaded = load_event_log_file(path);
  std::filesystem::remove(path);

  ASSERT_EQ(loaded.size(), original.size());
  for (std::size_t i = 0; i < original.size(); ++i) {
    EXPECT_EQ(loaded.at(i), original.at(i));
  }

  OrderBook first;
  OrderBook second;
  EXPECT_EQ(replay(first, original), replay(second, loaded));
  EXPECT_EQ(first.best_bid(), second.best_bid());
  EXPECT_EQ(first.best_ask(), second.best_ask());
}
