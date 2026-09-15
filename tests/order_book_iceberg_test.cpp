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
using mercury::Side;
using mercury::visible_quantity;
using mercury::jsonl::format_event_line;
using mercury::jsonl::parse_event_line;

TEST(Order, VisibleQuantityIceberg) {
  const Order full{.id = OrderId{1},
                   .side = Side::Buy,
                   .price = Price{100},
                   .quantity = Quantity{10}};
  EXPECT_EQ(visible_quantity(full), Quantity{10});

  const Order tip{.id = OrderId{2},
                  .side = Side::Buy,
                  .price = Price{100},
                  .quantity = Quantity{10},
                  .display = Quantity{3}};
  EXPECT_EQ(visible_quantity(tip), Quantity{3});
}

TEST(OrderBookIceberg, SnapshotShowsPeakOnly) {
  OrderBook book;
  book.add(Order{.id = OrderId{1},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{10},
                 .display = Quantity{2}});

  const auto snap = book.snapshot(1);
  ASSERT_EQ(snap.asks.size(), 1u);
  EXPECT_EQ(snap.asks[0].quantity, Quantity{2});
  EXPECT_EQ(snap.asks[0].order_count, 1u);
}

TEST(OrderBookIceberg, MatchConsumesHidden) {
  OrderBook book;
  book.add(Order{.id = OrderId{1},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{10},
                 .display = Quantity{2}});

  const auto trades = book.add(Order{.id = OrderId{2},
                                     .side = Side::Buy,
                                     .price = Price{100},
                                     .quantity = Quantity{7}});
  std::uint64_t filled = 0;
  for (const auto& trade : trades) {
    filled += trade.quantity.value();
    EXPECT_LE(trade.quantity.value(), 2u);
  }
  EXPECT_EQ(filled, 7u);

  const auto snap = book.snapshot(1);
  ASSERT_EQ(snap.asks.size(), 1u);
  // Last sweep took 1 of a 2-lot tip; tip not exhausted => visible 1 remains.
  EXPECT_EQ(snap.asks[0].quantity, Quantity{1});
}

TEST(OrderBookIceberg, TipRefillLosesPriority) {
  OrderBook book;
  book.add(Order{.id = OrderId{1},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{10},
                 .display = Quantity{2}});
  book.add(Order{.id = OrderId{2},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{1}});

  // Exhaust iceberg tip -> requeues behind order 2.
  const auto first = book.add(Order{.id = OrderId{3},
                                    .side = Side::Buy,
                                    .price = Price{100},
                                    .quantity = Quantity{2}});
  ASSERT_EQ(first.size(), 1u);
  EXPECT_EQ(first[0].maker_id, OrderId{1});

  const auto second = book.add(Order{.id = OrderId{4},
                                     .side = Side::Buy,
                                     .price = Price{100},
                                     .quantity = Quantity{1}});
  ASSERT_EQ(second.size(), 1u);
  EXPECT_EQ(second[0].maker_id, OrderId{2});
}

TEST(OrderBookIceberg, FokSeesHiddenLiquidity) {
  OrderBook book;
  book.add(Order{.id = OrderId{1},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{10},
                 .display = Quantity{1}});

  const auto trades = book.add(Order{.id = OrderId{2},
                                     .side = Side::Buy,
                                     .price = Price{100},
                                     .quantity = Quantity{10},
                                     .tif = mercury::TimeInForce::Fok});
  std::uint64_t filled = 0;
  for (const auto& trade : trades) {
    filled += trade.quantity.value();
  }
  EXPECT_EQ(filled, 10u);
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(EngineIceberg, ReplaceKeepsDisplay) {
  Engine engine;
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{10},
                           .account = AccountId{1},
                           .display = Quantity{2}})
                .decision,
            mercury::RiskDecision::Accept);

  const auto replaced = engine.replace(OrderId{1}, Price{101}, Quantity{8});
  ASSERT_TRUE(replaced.has_value());
  EXPECT_EQ(replaced->decision, mercury::RiskDecision::Accept);

  const auto snap = engine.book().snapshot(1);
  ASSERT_EQ(snap.asks.size(), 1u);
  EXPECT_EQ(snap.asks[0].price, Price{101});
  EXPECT_EQ(snap.asks[0].quantity, Quantity{2});
}

TEST(Jsonl, IcebergDisplayRoundTrip) {
  const Order order{.id = OrderId{3},
                    .side = Side::Buy,
                    .price = Price{50},
                    .quantity = Quantity{20},
                    .account = AccountId{1},
                    .display = Quantity{5}};
  const auto parsed = parse_event_line(format_event_line(order));
  ASSERT_TRUE(std::holds_alternative<Order>(parsed));
  EXPECT_EQ(std::get<Order>(parsed), order);
}
