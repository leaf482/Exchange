#pragma once

#include "mercury/order_book.hpp"
#include "mercury/positions.hpp"

namespace mercury {

constexpr Side opposite_side(Side side) {
  return side == Side::Buy ? Side::Sell : Side::Buy;
}

// OrderBook + Positions: applies maker/taker fills after each match.
class Engine {
 public:
  std::vector<Trade> add(Order order) {
    const Side taker_side = order.side;
    auto trades = book_.add(std::move(order));
    apply_trades(taker_side, trades);
    return trades;
  }

  std::vector<Trade> add_market(MarketOrder order) {
    const Side taker_side = order.side;
    auto trades = book_.add_market(std::move(order));
    apply_trades(taker_side, trades);
    return trades;
  }

  bool cancel(OrderId id) { return book_.cancel(id); }

  const OrderBook& book() const { return book_; }

  const Positions& positions() const { return positions_; }

 private:
  void apply_trades(Side taker_side, const std::vector<Trade>& trades) {
    const Side maker_side = opposite_side(taker_side);
    for (const Trade& trade : trades) {
      positions_.fill(trade.taker_account, taker_side, trade.price, trade.quantity);
      positions_.fill(trade.maker_account, maker_side, trade.price, trade.quantity);
    }
  }

  OrderBook book_;
  Positions positions_;
};

}  // namespace mercury
