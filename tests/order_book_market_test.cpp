#include "mercury/order_book.hpp"

#include <gtest/gtest.h>

using mercury::MarketOrder;
using mercury::Order;
using mercury::OrderBook;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;

namespace {

Order make_limit(std::uint64_t id, Side side, Price price, std::uint64_t qty) {
  return Order{
      .id = OrderId{id},
      .side = side,
      .price = price,
      .quantity = Quantity{qty},
  };
}

MarketOrder make_market(std::uint64_t id, Side side, std::uint64_t qty) {
  return MarketOrder{
      .id = OrderId{id},
      .side = side,
      .quantity = Quantity{qty},
  };
}

}  // namespace

TEST(OrderBookMarket, BuyFillsAvailableAsks) {
  OrderBook book;
  book.add(make_limit(1, Side::Sell, Price{100}, 3));
  book.add(make_limit(2, Side::Sell, Price{110}, 3));

  const auto trades = book.add_market(make_market(3, Side::Buy, 5));

  ASSERT_EQ(trades.size(), 2u);
  EXPECT_EQ(trades[0].price, Price{100});
  EXPECT_EQ(trades[0].quantity, Quantity{3});
  EXPECT_EQ(trades[1].price, Price{110});
  EXPECT_EQ(trades[1].quantity, Quantity{2});
  EXPECT_EQ(book.best_ask(), Price{110});
}

TEST(OrderBookMarket, SellFillsAvailableBids) {
  OrderBook book;
  book.add(make_limit(1, Side::Buy, Price{120}, 4));

  const auto trades = book.add_market(make_market(2, Side::Sell, 4));

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].price, Price{120});
  EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookMarket, DiscardsUnfilledQuantity) {
  OrderBook book;
  book.add(make_limit(1, Side::Sell, Price{100}, 2));

  const auto trades = book.add_market(make_market(2, Side::Buy, 5));

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].quantity, Quantity{2});
  EXPECT_FALSE(book.best_bid().has_value());
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookMarket, EmptyBookProducesNoTrades) {
  OrderBook book;

  const auto trades = book.add_market(make_market(1, Side::Buy, 5));

  EXPECT_TRUE(trades.empty());
  EXPECT_FALSE(book.best_bid().has_value());
  EXPECT_FALSE(book.best_ask().has_value());
}
