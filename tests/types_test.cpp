#include "mercury/types.hpp"

#include <gtest/gtest.h>

using mercury::OrderId;
using mercury::AccountId;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;

TEST(Types, SideValues) {
  EXPECT_NE(Side::Buy, Side::Sell);
}

TEST(Types, OrderIdValueAndEquality) {
  const OrderId a{1};
  const OrderId b{1};
  const OrderId c{2};

  EXPECT_EQ(a.value(), 1u);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_LT(a, c);
}

TEST(Types, AccountIdValueAndEquality) {
  const AccountId a{1};
  const AccountId b{1};
  const AccountId c{2};

  EXPECT_EQ(a.value(), 1u);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_LT(a, c);
}

TEST(Types, PriceTicksAndOrdering) {
  const Price low{100};
  const Price high{101};

  EXPECT_EQ(low.ticks(), 100);
  EXPECT_LT(low, high);
}

TEST(Types, QuantityValueAndOrdering) {
  const Quantity small{5};
  const Quantity large{10};

  EXPECT_EQ(small.value(), 5u);
  EXPECT_LT(small, large);
}
