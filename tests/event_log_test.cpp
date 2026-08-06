#include "mercury/event_log.hpp"

#include <gtest/gtest.h>

using mercury::CancelOrder;
using mercury::EventLog;
using mercury::MarketOrder;
using mercury::Order;
using mercury::OrderBook;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;
using mercury::Trade;
using mercury::apply;
using mercury::replay;

namespace {

Order make_limit(std::uint64_t id, Side side, Price price, std::uint64_t qty) {
  return Order{
      .id = OrderId{id},
      .side = side,
      .price = price,
      .quantity = Quantity{qty},
  };
}

}  // namespace

TEST(EventLog, AppendAndSize) {
  EventLog log;
  EXPECT_TRUE(log.empty());

  log.append(make_limit(1, Side::Buy, Price{100}, 5));
  log.append(CancelOrder{.id = OrderId{1}});

  EXPECT_EQ(log.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<Order>(log.at(0)));
  EXPECT_TRUE(std::holds_alternative<CancelOrder>(log.at(1)));
}

TEST(EventLog, ReplayMatchesDirectCalls) {
  EventLog log;
  log.append(make_limit(1, Side::Sell, Price{100}, 5));
  log.append(make_limit(2, Side::Buy, Price{100}, 5));

  OrderBook replayed;
  const auto replay_trades = replay(replayed, log);

  OrderBook direct;
  auto direct_trades = direct.add(make_limit(1, Side::Sell, Price{100}, 5));
  const auto fill = direct.add(make_limit(2, Side::Buy, Price{100}, 5));
  direct_trades.insert(direct_trades.end(), fill.begin(), fill.end());

  EXPECT_EQ(replay_trades, direct_trades);
  EXPECT_EQ(replayed.best_bid(), direct.best_bid());
  EXPECT_EQ(replayed.best_ask(), direct.best_ask());
}

TEST(EventLog, ReplayIsDeterministic) {
  EventLog log;
  log.append(make_limit(1, Side::Sell, Price{100}, 3));
  log.append(make_limit(2, Side::Sell, Price{110}, 3));
  log.append(MarketOrder{.id = OrderId{3}, .side = Side::Buy, .quantity = Quantity{5}});
  log.append(CancelOrder{.id = OrderId{2}});

  OrderBook first_book;
  const auto first = replay(first_book, log);

  OrderBook second_book;
  const auto second = replay(second_book, log);

  EXPECT_EQ(first, second);
  EXPECT_EQ(first_book.best_bid(), second_book.best_bid());
  EXPECT_EQ(first_book.best_ask(), second_book.best_ask());
}

TEST(EventLog, ApplyCancelRemovesRestingOrder) {
  OrderBook book;
  EventLog log;
  log.append(make_limit(1, Side::Buy, Price{100}, 5));

  replay(book, log);
  EXPECT_EQ(book.best_bid(), Price{100});

  const auto trades = apply(book, CancelOrder{.id = OrderId{1}});
  EXPECT_TRUE(trades.empty());
  EXPECT_FALSE(book.best_bid().has_value());
}
