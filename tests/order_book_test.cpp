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

TEST(OrderBook, StartsWithNoBestPrices) {
  const OrderBook book;

  EXPECT_FALSE(book.best_bid().has_value());
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBook, AddBuySetsBestBid) {
  OrderBook book;
  book.add(make_order(1, Side::Buy, Price{100}, 5));

  EXPECT_EQ(book.best_bid(), Price{100});
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBook, AddSellSetsBestAsk) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{200}, 5));

  EXPECT_EQ(book.best_ask(), Price{200});
  EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBook, BestBidIsHighestPrice) {
  OrderBook book;
  book.add(make_order(1, Side::Buy, Price{100}, 5));
  book.add(make_order(2, Side::Buy, Price{120}, 5));
  book.add(make_order(3, Side::Buy, Price{110}, 5));

  EXPECT_EQ(book.best_bid(), Price{120});
}

TEST(OrderBook, BestAskIsLowestPrice) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{200}, 5));
  book.add(make_order(2, Side::Sell, Price{180}, 5));
  book.add(make_order(3, Side::Sell, Price{190}, 5));

  EXPECT_EQ(book.best_ask(), Price{180});
}

TEST(OrderBook, SamePriceJoinsLevel) {
  OrderBook book;
  book.add(make_order(1, Side::Buy, Price{100}, 5));
  book.add(make_order(2, Side::Buy, Price{100}, 7));

  EXPECT_EQ(book.best_bid(), Price{100});
}
