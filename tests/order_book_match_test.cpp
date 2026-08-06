#include "mercury/order_book.hpp"

#include <gtest/gtest.h>

using mercury::Order;
using mercury::OrderBook;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;
using mercury::Trade;

namespace {

Order make_order(std::uint64_t id, Side side, Price price, std::uint64_t qty) {
  return Order{
      .id = OrderId{id},
      .side = side,
      .price = price,
      .quantity = Quantity{qty},
  };
}

}  // namespace

TEST(OrderBookMatch, NonCrossingRestsWithoutTrade) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{200}, 5));

  const auto trades = book.add(make_order(2, Side::Buy, Price{100}, 5));

  EXPECT_TRUE(trades.empty());
  EXPECT_EQ(book.best_bid(), Price{100});
  EXPECT_EQ(book.best_ask(), Price{200});
}

TEST(OrderBookMatch, FullFillAtMakerPrice) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 5));

  const auto trades = book.add(make_order(2, Side::Buy, Price{120}, 5));

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0], (Trade{
                           .maker_id = OrderId{1},
                           .taker_id = OrderId{2},
                           .price = Price{100},
                           .quantity = Quantity{5},
                       }));
  EXPECT_FALSE(book.best_bid().has_value());
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookMatch, PartialFillRestsRemainder) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 3));

  const auto trades = book.add(make_order(2, Side::Buy, Price{100}, 5));

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].quantity, Quantity{3});
  EXPECT_EQ(book.best_bid(), Price{100});
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookMatch, PartialFillLeavesRestingOrder) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 5));

  const auto trades = book.add(make_order(2, Side::Buy, Price{100}, 2));

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].quantity, Quantity{2});
  EXPECT_FALSE(book.best_bid().has_value());
  EXPECT_EQ(book.best_ask(), Price{100});

  const auto trades2 = book.add(make_order(3, Side::Buy, Price{100}, 3));
  ASSERT_EQ(trades2.size(), 1u);
  EXPECT_EQ(trades2[0].maker_id, OrderId{1});
  EXPECT_EQ(trades2[0].quantity, Quantity{3});
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookMatch, TimePriorityAtSamePrice) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 5));
  book.add(make_order(2, Side::Sell, Price{100}, 5));

  const auto trades = book.add(make_order(3, Side::Buy, Price{100}, 5));

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].maker_id, OrderId{1});
  EXPECT_EQ(book.best_ask(), Price{100});

  const auto trades2 = book.add(make_order(4, Side::Buy, Price{100}, 5));
  ASSERT_EQ(trades2.size(), 1u);
  EXPECT_EQ(trades2[0].maker_id, OrderId{2});
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookMatch, WalksMultipleAskLevels) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 2));
  book.add(make_order(2, Side::Sell, Price{110}, 2));

  const auto trades = book.add(make_order(3, Side::Buy, Price{110}, 4));

  ASSERT_EQ(trades.size(), 2u);
  EXPECT_EQ(trades[0].price, Price{100});
  EXPECT_EQ(trades[0].quantity, Quantity{2});
  EXPECT_EQ(trades[1].price, Price{110});
  EXPECT_EQ(trades[1].quantity, Quantity{2});
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookMatch, SellMatchesBestBid) {
  OrderBook book;
  book.add(make_order(1, Side::Buy, Price{120}, 5));

  const auto trades = book.add(make_order(2, Side::Sell, Price{100}, 5));

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].price, Price{120});
  EXPECT_FALSE(book.best_bid().has_value());
  EXPECT_FALSE(book.best_ask().has_value());
}
