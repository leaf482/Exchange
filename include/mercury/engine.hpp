#pragma once

#include "mercury/order_book.hpp"
#include "mercury/positions.hpp"
#include "mercury/risk.hpp"

#include <utility>
#include <vector>

namespace mercury {

constexpr Side opposite_side(Side side) {
  return side == Side::Buy ? Side::Sell : Side::Buy;
}

struct SubmitResult {
  RiskDecision decision{RiskDecision::Accept};
  std::vector<Trade> trades{};
};

// OrderBook + Positions with optional pre-trade risk checks.
class Engine {
 public:
  explicit Engine(RiskLimits limits = {}) : limits_(limits) {}

  SubmitResult add(Order order) {
    const RiskDecision decision = check_order(
        limits_, positions_, order.account, order.side, order.quantity);
    if (decision != RiskDecision::Accept) {
      return SubmitResult{.decision = decision};
    }

    const Side taker_side = order.side;
    auto trades = book_.add(std::move(order));
    apply_trades(taker_side, trades);
    return SubmitResult{.decision = RiskDecision::Accept, .trades = std::move(trades)};
  }

  SubmitResult add_market(MarketOrder order) {
    const RiskDecision decision = check_order(
        limits_, positions_, order.account, order.side, order.quantity);
    if (decision != RiskDecision::Accept) {
      return SubmitResult{.decision = decision};
    }

    const Side taker_side = order.side;
    auto trades = book_.add_market(std::move(order));
    apply_trades(taker_side, trades);
    return SubmitResult{.decision = RiskDecision::Accept, .trades = std::move(trades)};
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

  RiskLimits limits_;
  OrderBook book_;
  Positions positions_;
};

}  // namespace mercury
