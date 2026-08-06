#include "mercury/price_level.hpp"

#include <gtest/gtest.h>

using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::PriceLevel;
using mercury::Quantity;
using mercury::Side;

namespace {

Order make_order(std::uint64_t id, Price price, std::uint64_t qty) {
  return Order{
      .id = OrderId{id},
      .side = Side::Buy,
      .price = price,
      .quantity = Quantity{qty},
  };
}

}  // namespace

TEST(PriceLevel, StartsEmpty) {
  const PriceLevel level{Price{100}};

  EXPECT_EQ(level.price(), Price{100});
  EXPECT_TRUE(level.empty());
  EXPECT_EQ(level.size(), 0u);
}

TEST(PriceLevel, EnqueueSetsFront) {
  PriceLevel level{Price{100}};
  level.enqueue(make_order(1, Price{100}, 5));

  EXPECT_FALSE(level.empty());
  EXPECT_EQ(level.size(), 1u);
  EXPECT_EQ(level.front().id, OrderId{1});
  EXPECT_EQ(level.front().quantity, Quantity{5});
}

TEST(PriceLevel, FifoOrder) {
  PriceLevel level{Price{100}};
  level.enqueue(make_order(1, Price{100}, 5));
  level.enqueue(make_order(2, Price{100}, 7));

  EXPECT_EQ(level.size(), 2u);
  EXPECT_EQ(level.front().id, OrderId{1});

  level.dequeue();

  EXPECT_EQ(level.size(), 1u);
  EXPECT_EQ(level.front().id, OrderId{2});

  level.dequeue();

  EXPECT_TRUE(level.empty());
}
