#include "mercury/order_book.hpp"

#include <gtest/gtest.h>

using mercury::Order;
using mercury::OrderBook;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;
using mercury::TimeInForce;

namespace {

Order make_order(std::uint64_t id, Side side, Price price, std::uint64_t qty,
                 TimeInForce tif = TimeInForce::Gtc) {
  return Order{
      .id = OrderId{id},
      .side = side,
      .price = price,
      .quantity = Quantity{qty},
      .tif = tif,
  };
}

}  // namespace

TEST(OrderBookTif, IocFillsAndDiscardsRemainder) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 3));

  const auto trades =
      book.add(make_order(2, Side::Buy, Price{100}, 5, TimeInForce::Ioc));

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].quantity, Quantity{3});
  EXPECT_FALSE(book.best_bid().has_value());
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookTif, IocDoesNotRestWhenNoLiquidity) {
  OrderBook book;

  const auto trades =
      book.add(make_order(1, Side::Buy, Price{100}, 5, TimeInForce::Ioc));

  EXPECT_TRUE(trades.empty());
  EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookTif, FokFullyFills) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 2));
  book.add(make_order(2, Side::Sell, Price{101}, 3));

  const auto trades =
      book.add(make_order(3, Side::Buy, Price{101}, 5, TimeInForce::Fok));

  ASSERT_EQ(trades.size(), 2u);
  EXPECT_EQ(trades[0].quantity, Quantity{2});
  EXPECT_EQ(trades[1].quantity, Quantity{3});
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookTif, FokRejectsPartialAvailability) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 3));

  const auto trades =
      book.add(make_order(2, Side::Buy, Price{100}, 5, TimeInForce::Fok));

  EXPECT_TRUE(trades.empty());
  EXPECT_EQ(book.best_ask(), Price{100});
  EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookTif, GtcStillRestsRemainder) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 3));

  const auto trades = book.add(make_order(2, Side::Buy, Price{100}, 5));

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(book.best_bid(), Price{100});
}
