#include "mercury/positions.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Positions;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;

TEST(Positions, BuyOpensLong) {
  Positions positions;
  positions.fill(AccountId{1}, Side::Buy, Price{100}, Quantity{5});

  EXPECT_EQ(positions.quantity(AccountId{1}), 5);
  EXPECT_EQ(positions.realized_pnl(AccountId{1}), 0);
  EXPECT_EQ(positions.unrealized_pnl(AccountId{1}, Price{110}), 50);
}

TEST(Positions, SellOpensShort) {
  Positions positions;
  positions.fill(AccountId{1}, Side::Sell, Price{100}, Quantity{5});

  EXPECT_EQ(positions.quantity(AccountId{1}), -5);
  EXPECT_EQ(positions.unrealized_pnl(AccountId{1}, Price{90}), 50);
}

TEST(Positions, RealizeLongProfit) {
  Positions positions;
  positions.fill(AccountId{1}, Side::Buy, Price{100}, Quantity{5});
  positions.fill(AccountId{1}, Side::Sell, Price{110}, Quantity{5});

  EXPECT_EQ(positions.quantity(AccountId{1}), 0);
  EXPECT_EQ(positions.realized_pnl(AccountId{1}), 50);
  EXPECT_EQ(positions.unrealized_pnl(AccountId{1}, Price{120}), 0);
}

TEST(Positions, RealizeShortProfit) {
  Positions positions;
  positions.fill(AccountId{1}, Side::Sell, Price{100}, Quantity{5});
  positions.fill(AccountId{1}, Side::Buy, Price{90}, Quantity{5});

  EXPECT_EQ(positions.quantity(AccountId{1}), 0);
  EXPECT_EQ(positions.realized_pnl(AccountId{1}), 50);
}

TEST(Positions, PartialCloseKeepsAverage) {
  Positions positions;
  positions.fill(AccountId{1}, Side::Buy, Price{100}, Quantity{10});
  positions.fill(AccountId{1}, Side::Sell, Price{120}, Quantity{4});

  EXPECT_EQ(positions.quantity(AccountId{1}), 6);
  EXPECT_EQ(positions.realized_pnl(AccountId{1}), 80);
  EXPECT_EQ(positions.unrealized_pnl(AccountId{1}, Price{110}), 60);
}

TEST(Positions, FlipUpdatesAverage) {
  Positions positions;
  positions.fill(AccountId{1}, Side::Buy, Price{100}, Quantity{5});
  positions.fill(AccountId{1}, Side::Sell, Price{110}, Quantity{8});

  EXPECT_EQ(positions.quantity(AccountId{1}), -3);
  EXPECT_EQ(positions.realized_pnl(AccountId{1}), 50);
  EXPECT_EQ(positions.unrealized_pnl(AccountId{1}, Price{100}), 30);
}

TEST(Positions, AccountsAreIndependent) {
  Positions positions;
  positions.fill(AccountId{1}, Side::Buy, Price{100}, Quantity{5});
  positions.fill(AccountId{2}, Side::Sell, Price{100}, Quantity{3});

  EXPECT_EQ(positions.quantity(AccountId{1}), 5);
  EXPECT_EQ(positions.quantity(AccountId{2}), -3);
  EXPECT_EQ(positions.quantity(AccountId{3}), 0);
}

TEST(Positions, AverageCostOnAdd) {
  Positions positions;
  positions.fill(AccountId{1}, Side::Buy, Price{100}, Quantity{2});
  positions.fill(AccountId{1}, Side::Buy, Price{120}, Quantity{2});

  EXPECT_EQ(positions.quantity(AccountId{1}), 4);
  EXPECT_EQ(positions.unrealized_pnl(AccountId{1}, Price{110}), 0);
}
