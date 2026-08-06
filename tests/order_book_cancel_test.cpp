#include "mercury/order_book.hpp"

#include <gtest/gtest.h>

using mercury::Order;
using mercury::OrderBook;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;

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

TEST(OrderBookCancel, CancelsRestingOrder) {
  OrderBook book;
  book.add(make_order(1, Side::Buy, Price{100}, 5));

  EXPECT_TRUE(book.cancel(OrderId{1}));
  EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookCancel, UnknownIdReturnsFalse) {
  OrderBook book;
  book.add(make_order(1, Side::Buy, Price{100}, 5));

  EXPECT_FALSE(book.cancel(OrderId{99}));
  EXPECT_EQ(book.best_bid(), Price{100});
}

TEST(OrderBookCancel, PreservesOtherOrdersAtSamePrice) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 5));
  book.add(make_order(2, Side::Sell, Price{100}, 7));

  EXPECT_TRUE(book.cancel(OrderId{1}));
  EXPECT_EQ(book.best_ask(), Price{100});

  const auto trades = book.add(make_order(3, Side::Buy, Price{100}, 7));
  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].maker_id, OrderId{2});
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookCancel, FilledOrderCannotBeCancelled) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 5));
  book.add(make_order(2, Side::Buy, Price{100}, 5));

  EXPECT_FALSE(book.cancel(OrderId{1}));
}

TEST(OrderBookCancel, CancelBestRevealsNextLevel) {
  OrderBook book;
  book.add(make_order(1, Side::Buy, Price{120}, 5));
  book.add(make_order(2, Side::Buy, Price{100}, 5));

  EXPECT_TRUE(book.cancel(OrderId{1}));
  EXPECT_EQ(book.best_bid(), Price{100});
}
