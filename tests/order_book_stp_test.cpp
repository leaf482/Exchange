#include "mercury/order_book.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::MarketOrder;
using mercury::Order;
using mercury::OrderBook;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::SelfTradePrevention;
using mercury::Side;
using mercury::TimeInForce;

TEST(OrderBookStp, CancelRestingSkipsSelfAndContinues) {
  OrderBook book{SelfTradePrevention::CancelResting};

  book.add(Order{.id = OrderId{1},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{5},
                 .account = AccountId{7}});
  book.add(Order{.id = OrderId{2},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{4},
                 .account = AccountId{8}});

  const auto trades = book.add(Order{.id = OrderId{3},
                                     .side = Side::Buy,
                                     .price = Price{100},
                                     .quantity = Quantity{4},
                                     .account = AccountId{7}});

  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].maker_id, OrderId{2});
  EXPECT_EQ(trades[0].quantity, Quantity{4});

  const auto cancelled = book.take_stp_cancels();
  ASSERT_EQ(cancelled.size(), 1u);
  EXPECT_EQ(cancelled[0], OrderId{1});
  EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookStp, AccountZeroDoesNotTrigger) {
  OrderBook book{SelfTradePrevention::CancelResting};
  book.add(Order{.id = OrderId{1},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{5},
                 .account = AccountId{0}});
  const auto trades = book.add(Order{.id = OrderId{2},
                                     .side = Side::Buy,
                                     .price = Price{100},
                                     .quantity = Quantity{5},
                                     .account = AccountId{0}});
  ASSERT_EQ(trades.size(), 1u);
  EXPECT_TRUE(book.take_stp_cancels().empty());
}

TEST(OrderBookStp, OffAllowsSelfTrade) {
  OrderBook book{SelfTradePrevention::Off};
  book.add(Order{.id = OrderId{1},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{5},
                 .account = AccountId{7}});
  const auto trades = book.add(Order{.id = OrderId{2},
                                     .side = Side::Buy,
                                     .price = Price{100},
                                     .quantity = Quantity{5},
                                     .account = AccountId{7}});
  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].maker_account, AccountId{7});
}

TEST(OrderBookStp, FokIgnoresSelfLiquidity) {
  OrderBook book{SelfTradePrevention::CancelResting};
  book.add(Order{.id = OrderId{1},
                 .side = Side::Sell,
                 .price = Price{100},
                 .quantity = Quantity{5},
                 .account = AccountId{7}});
  const auto trades = book.add(Order{.id = OrderId{2},
                                     .side = Side::Buy,
                                     .price = Price{100},
                                     .quantity = Quantity{5},
                                     .account = AccountId{7},
                                     .tif = TimeInForce::Fok});
  EXPECT_TRUE(trades.empty());
  EXPECT_EQ(book.best_ask(), Price{100});
}

TEST(OrderBookStp, MarketCancelsRestingSelf) {
  OrderBook book{SelfTradePrevention::CancelResting};
  book.add(Order{.id = OrderId{1},
                 .side = Side::Buy,
                 .price = Price{100},
                 .quantity = Quantity{3},
                 .account = AccountId{9}});
  book.add(Order{.id = OrderId{2},
                 .side = Side::Buy,
                 .price = Price{100},
                 .quantity = Quantity{2},
                 .account = AccountId{1}});

  const auto trades =
      book.add_market(MarketOrder{.id = OrderId{3},
                                  .side = Side::Sell,
                                  .quantity = Quantity{2},
                                  .account = AccountId{9}});
  ASSERT_EQ(trades.size(), 1u);
  EXPECT_EQ(trades[0].maker_id, OrderId{2});
  EXPECT_EQ(book.take_stp_cancels().size(), 1u);
}
