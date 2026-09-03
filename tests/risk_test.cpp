#include "mercury/risk.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Positions;
using mercury::Price;
using mercury::Quantity;
using mercury::RiskDecision;
using mercury::RiskLimits;
using mercury::Side;
using mercury::check_order;

TEST(Risk, AcceptsWithinLimits) {
  const RiskLimits limits{
      .max_order_quantity = Quantity{10},
      .max_abs_position = 20,
  };
  const Positions positions;

  EXPECT_EQ(check_order(limits, positions, AccountId{1}, Side::Buy, Quantity{5}),
            RiskDecision::Accept);
}

TEST(Risk, RejectsZeroQuantity) {
  const RiskLimits limits{};
  const Positions positions;

  EXPECT_EQ(check_order(limits, positions, AccountId{1}, Side::Buy, Quantity{0}),
            RiskDecision::OrderTooLarge);
}

TEST(Risk, RejectsOrderTooLarge) {
  const RiskLimits limits{.max_order_quantity = Quantity{10}};
  const Positions positions;

  EXPECT_EQ(check_order(limits, positions, AccountId{1}, Side::Buy, Quantity{11}),
            RiskDecision::OrderTooLarge);
}

TEST(Risk, RejectsPositionLimit) {
  RiskLimits limits{.max_abs_position = 10};
  Positions positions;
  positions.fill(AccountId{1}, Side::Buy, Price{100}, Quantity{8});

  EXPECT_EQ(check_order(limits, positions, AccountId{1}, Side::Buy, Quantity{3}),
            RiskDecision::PositionLimit);
}

TEST(Risk, AllowsReducingPosition) {
  RiskLimits limits{.max_abs_position = 10};
  Positions positions;
  positions.fill(AccountId{1}, Side::Buy, Price{100}, Quantity{10});

  EXPECT_EQ(check_order(limits, positions, AccountId{1}, Side::Sell, Quantity{10}),
            RiskDecision::Accept);
}
