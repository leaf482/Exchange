#include "mercury/order.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;

TEST(Order, StoresFields) {
  const Order order{
      .id = OrderId{42},
      .side = Side::Buy,
      .price = Price{1000},
      .quantity = Quantity{5},
      .account = AccountId{7},
  };

  EXPECT_EQ(order.id, OrderId{42});
  EXPECT_EQ(order.side, Side::Buy);
  EXPECT_EQ(order.price, Price{1000});
  EXPECT_EQ(order.quantity, Quantity{5});
  EXPECT_EQ(order.account, AccountId{7});
}

TEST(Order, Equality) {
  const Order a{
      .id = OrderId{1},
      .side = Side::Sell,
      .price = Price{200},
      .quantity = Quantity{3},
  };
  const Order b = a;
  const Order c{
      .id = OrderId{2},
      .side = Side::Sell,
      .price = Price{200},
      .quantity = Quantity{3},
  };

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}
