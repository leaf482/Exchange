#include "mercury/engine.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::AccountReport;
using mercury::Engine;
using mercury::MarkSource;
using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::RiskDecision;
using mercury::Side;
using mercury::Symbol;

TEST(EngineAccountReport, CashFeesAndEquity) {
  Engine engine;
  engine.set_cash(AccountId{1}, 10'000);

  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{10},
                           .account = AccountId{2}})
                .decision,
            RiskDecision::Accept);
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{2},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{4},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);

  // Buy 4 @ 100 -> cash 10000-400=9600, long 4, mark 100
  const AccountReport report = engine.account_report(AccountId{1});
  EXPECT_EQ(report.cash, 9600);
  EXPECT_EQ(report.reserved, 0);
  EXPECT_EQ(report.available, 9600);
  EXPECT_EQ(report.fees_paid, 0);
  ASSERT_EQ(report.positions.size(), 1u);
  EXPECT_EQ(report.positions[0].symbol, Symbol{0});
  EXPECT_EQ(report.positions[0].quantity, 4);
  EXPECT_EQ(report.positions[0].avg_ticks, 100);
  EXPECT_EQ(report.positions[0].mark_ticks, 100);
  EXPECT_EQ(report.positions[0].unrealized_pnl, 0);
  EXPECT_EQ(report.realized_pnl, 0);
  EXPECT_EQ(report.unrealized_pnl, 0);
  EXPECT_EQ(report.inventory_mark, 400);
  EXPECT_EQ(report.equity, 10'000);
}

TEST(EngineAccountReport, UnrealizedUsesMark) {
  Engine engine;
  engine.add(Order{.id = OrderId{1},
                   .side = Side::Sell,
                   .price = Price{100},
                   .quantity = Quantity{5},
                   .account = AccountId{2}});
  engine.add(Order{.id = OrderId{2},
                   .side = Side::Buy,
                   .price = Price{100},
                   .quantity = Quantity{5},
                   .account = AccountId{1}});
  // Mark moves via another trade at 110
  engine.add(Order{.id = OrderId{3},
                   .side = Side::Sell,
                   .price = Price{110},
                   .quantity = Quantity{1},
                   .account = AccountId{2}});
  engine.add(Order{.id = OrderId{4},
                   .side = Side::Buy,
                   .price = Price{110},
                   .quantity = Quantity{1},
                   .account = AccountId{3}});

  const AccountReport report =
      engine.account_report(AccountId{1}, MarkSource::LastTrade);
  ASSERT_EQ(report.positions.size(), 1u);
  EXPECT_EQ(report.positions[0].quantity, 5);
  EXPECT_EQ(report.positions[0].unrealized_pnl, 50);  // (110-100)*5
  EXPECT_EQ(report.unrealized_pnl, 50);
  EXPECT_EQ(report.inventory_mark, 550);
}

TEST(EngineAccountReport, MidMarkWhenTwoSided) {
  Engine engine;
  engine.add(Order{.id = OrderId{1},
                   .side = Side::Buy,
                   .price = Price{90},
                   .quantity = Quantity{1},
                   .account = AccountId{1}});
  engine.add(Order{.id = OrderId{2},
                   .side = Side::Sell,
                   .price = Price{110},
                   .quantity = Quantity{1},
                   .account = AccountId{2}});
  // Seed last-trade position for account 3 via match at 100
  engine.add(Order{.id = OrderId{3},
                   .side = Side::Sell,
                   .price = Price{100},
                   .quantity = Quantity{2},
                   .account = AccountId{2}});
  engine.add(Order{.id = OrderId{4},
                   .side = Side::Buy,
                   .price = Price{100},
                   .quantity = Quantity{2},
                   .account = AccountId{3}});

  // Restore two-sided book around mid 100
  engine.add(Order{.id = OrderId{5},
                   .side = Side::Buy,
                   .price = Price{95},
                   .quantity = Quantity{1},
                   .account = AccountId{1}});
  engine.add(Order{.id = OrderId{6},
                   .side = Side::Sell,
                   .price = Price{105},
                   .quantity = Quantity{1},
                   .account = AccountId{2}});

  const AccountReport mid = engine.account_report(AccountId{3}, MarkSource::Mid);
  ASSERT_EQ(mid.positions.size(), 1u);
  EXPECT_EQ(mid.positions[0].mark_ticks, 100);  // (95+105)/2
  EXPECT_EQ(mid.positions[0].unrealized_pnl, 0);
}
