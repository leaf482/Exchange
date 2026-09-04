#include "mercury/order_book.hpp"

#include <gtest/gtest.h>

using mercury::BookLevel;
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

TEST(BookSnapshot, EmptyBook) {
  const OrderBook book;
  const auto snap = book.snapshot(5);

  EXPECT_TRUE(snap.bids.empty());
  EXPECT_TRUE(snap.asks.empty());
  EXPECT_FALSE(snap.spread_ticks().has_value());
}

TEST(BookSnapshot, DepthAndOrdering) {
  OrderBook book;
  book.add(make_order(1, Side::Buy, Price{100}, 5));
  book.add(make_order(2, Side::Buy, Price{110}, 2));
  book.add(make_order(3, Side::Buy, Price{100}, 3));
  book.add(make_order(4, Side::Sell, Price{120}, 4));
  book.add(make_order(5, Side::Sell, Price{130}, 1));
  book.add(make_order(6, Side::Sell, Price{125}, 2));

  const auto snap = book.snapshot(2);

  ASSERT_EQ(snap.bids.size(), 2u);
  EXPECT_EQ(snap.bids[0], (BookLevel{Price{110}, Quantity{2}, 1}));
  EXPECT_EQ(snap.bids[1], (BookLevel{Price{100}, Quantity{8}, 2}));

  ASSERT_EQ(snap.asks.size(), 2u);
  EXPECT_EQ(snap.asks[0], (BookLevel{Price{120}, Quantity{4}, 1}));
  EXPECT_EQ(snap.asks[1], (BookLevel{Price{125}, Quantity{2}, 1}));

  EXPECT_EQ(snap.best_bid(), Price{110});
  EXPECT_EQ(snap.best_ask(), Price{120});
  EXPECT_EQ(snap.spread_ticks(), 10);
}

TEST(BookSnapshot, RespectsMaxLevels) {
  OrderBook book;
  book.add(make_order(1, Side::Sell, Price{100}, 1));
  book.add(make_order(2, Side::Sell, Price{101}, 1));
  book.add(make_order(3, Side::Sell, Price{102}, 1));

  const auto snap = book.snapshot(1);
  ASSERT_EQ(snap.asks.size(), 1u);
  EXPECT_EQ(snap.asks[0].price, Price{100});
  EXPECT_TRUE(snap.bids.empty());
}
