#include "mercury/engine.hpp"
#include "mercury/event_log.hpp"
#include "mercury/fees.hpp"
#include "mercury/jsonl.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Engine;
using mercury::EventLog;
using mercury::FeeSchedule;
using mercury::MassCancelFilter;
using mercury::MassCancelOrder;
using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::RiskDecision;
using mercury::Side;
using mercury::StopOrder;
using mercury::Symbol;
using mercury::jsonl::format_event_line;
using mercury::jsonl::parse_event_line;
using mercury::replay;

TEST(EngineFees, MakerRebateTakerFee) {
  Engine engine{{}, {}, FeeSchedule{.maker_bps = -10, .taker_bps = 20}};

  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Sell,
                           .price = Price{1000},
                           .quantity = Quantity{10},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
  const auto fill =
      engine.add(Order{.id = OrderId{2},
                       .side = Side::Buy,
                       .price = Price{1000},
                       .quantity = Quantity{10},
                       .account = AccountId{2}});
  ASSERT_EQ(fill.trades.size(), 1u);
  // notional = 1000 * 10 = 10000
  EXPECT_EQ(fill.trades[0].maker_fee, -10);  // -10 bps
  EXPECT_EQ(fill.trades[0].taker_fee, 20);   // +20 bps
  EXPECT_EQ(engine.fees_paid(AccountId{1}), -10);
  EXPECT_EQ(engine.fees_paid(AccountId{2}), 20);
}

TEST(EngineMassCancel, FiltersByAccountAndSide) {
  Engine engine;
  engine.add(Order{.id = OrderId{1}, .side = Side::Buy, .price = Price{100},
                   .quantity = Quantity{1}, .account = AccountId{1}, .symbol = Symbol{0}});
  engine.add(Order{.id = OrderId{2}, .side = Side::Sell, .price = Price{110},
                   .quantity = Quantity{1}, .account = AccountId{1}, .symbol = Symbol{0}});
  engine.add(Order{.id = OrderId{3}, .side = Side::Buy, .price = Price{99},
                   .quantity = Quantity{1}, .account = AccountId{2}, .symbol = Symbol{0}});

  EXPECT_EQ(engine.mass_cancel(MassCancelFilter{.account = AccountId{1},
                                               .symbol = std::nullopt,
                                               .side = Side::Buy}),
            1u);
  ASSERT_TRUE(engine.book().best_bid());
  EXPECT_EQ(*engine.book().best_bid(), Price{99});
  EXPECT_EQ(engine.book().best_ask(), Price{110});
}

TEST(EngineMassCancel, IncludesStopsAndSymbol) {
  Engine engine;
  engine.add(Order{.id = OrderId{1}, .side = Side::Buy, .price = Price{100},
                   .quantity = Quantity{1}, .account = AccountId{1}, .symbol = Symbol{1}});
  engine.add_stop(StopOrder{.id = OrderId{2},
                            .side = Side::Sell,
                            .stop_price = Price{90},
                            .quantity = Quantity{1},
                            .account = AccountId{1},
                            .limit_price = std::nullopt,
                            .symbol = Symbol{1}});
  engine.add(Order{.id = OrderId{3}, .side = Side::Buy, .price = Price{100},
                   .quantity = Quantity{1}, .account = AccountId{1}, .symbol = Symbol{2}});

  EXPECT_EQ(engine.mass_cancel(MassCancelFilter{.account = std::nullopt,
                                               .symbol = Symbol{1},
                                               .side = std::nullopt}),
            2u);
  EXPECT_EQ(engine.pending_stop_count(Symbol{1}), 0u);
  EXPECT_EQ(engine.book(Symbol{2}).best_bid(), Price{100});
}

TEST(Jsonl, MassCancelRoundTrip) {
  const MassCancelOrder event{.filter = MassCancelFilter{
                                  .account = AccountId{7},
                                  .symbol = Symbol{3},
                                  .side = Side::Sell,
                              }};
  const auto parsed = parse_event_line(format_event_line(event));
  ASSERT_TRUE(std::holds_alternative<MassCancelOrder>(parsed));
  EXPECT_EQ(std::get<MassCancelOrder>(parsed), event);
}

TEST(EventLog, MassCancelReplay) {
  EventLog log;
  log.append(Order{.id = OrderId{1}, .side = Side::Buy, .price = Price{100},
                   .quantity = Quantity{1}, .account = AccountId{1}});
  log.append(Order{.id = OrderId{2}, .side = Side::Buy, .price = Price{99},
                   .quantity = Quantity{1}, .account = AccountId{2}});
  log.append(MassCancelOrder{.filter = MassCancelFilter{.account = AccountId{1},
                                                       .symbol = std::nullopt,
                                                       .side = std::nullopt}});

  Engine engine;
  replay(engine, log);
  ASSERT_TRUE(engine.book().best_bid());
  EXPECT_EQ(*engine.book().best_bid(), Price{99});
}
