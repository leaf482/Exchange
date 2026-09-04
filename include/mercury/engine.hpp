#pragma once

#include "mercury/order_book.hpp"
#include "mercury/positions.hpp"
#include "mercury/risk.hpp"

#include <cstdint>
#include <map>
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
    const AccountId account = order.account;
    const WorkingExposure exposure = working_for(account);
    const RiskDecision decision = check_order(limits_, positions_, account, order.side,
                                              order.quantity, exposure.buy, exposure.sell);
    if (decision != RiskDecision::Accept) {
      return SubmitResult{.decision = decision};
    }

    const OrderId id = order.id;
    const Side taker_side = order.side;
    const Quantity original = order.quantity;
    auto trades = book_.add(std::move(order));

    Quantity filled{0};
    for (const Trade& trade : trades) {
      filled = Quantity{filled.value() + trade.quantity.value()};
      reduce_open(trade.maker_id, trade.quantity);
    }

    const Quantity rested{original.value() - filled.value()};
    if (!rested.is_zero()) {
      add_open(id, account, taker_side, rested);
    }

    apply_trades(taker_side, trades);
    return SubmitResult{.decision = RiskDecision::Accept, .trades = std::move(trades)};
  }

  SubmitResult add_market(MarketOrder order) {
    const AccountId account = order.account;
    const WorkingExposure exposure = working_for(account);
    const RiskDecision decision = check_order(limits_, positions_, account, order.side,
                                              order.quantity, exposure.buy, exposure.sell);
    if (decision != RiskDecision::Accept) {
      return SubmitResult{.decision = decision};
    }

    const Side taker_side = order.side;
    auto trades = book_.add_market(std::move(order));
    for (const Trade& trade : trades) {
      reduce_open(trade.maker_id, trade.quantity);
    }
    apply_trades(taker_side, trades);
    return SubmitResult{.decision = RiskDecision::Accept, .trades = std::move(trades)};
  }

  bool cancel(OrderId id) {
    const auto open_it = open_orders_.find(id);
    if (open_it != open_orders_.end()) {
      reduce_open(id, open_it->second.remaining);
    }
    return book_.cancel(id);
  }

  BookSnapshot snapshot(std::size_t max_levels) const { return book_.snapshot(max_levels); }

  const OrderBook& book() const { return book_; }

  const Positions& positions() const { return positions_; }

 private:
  struct WorkingExposure {
    std::uint64_t buy = 0;
    std::uint64_t sell = 0;
  };

  struct OpenOrder {
    AccountId account;
    Side side;
    Quantity remaining;
  };

  WorkingExposure working_for(AccountId account) const {
    const auto it = working_.find(account);
    return it == working_.end() ? WorkingExposure{} : it->second;
  }

  void add_open(OrderId id, AccountId account, Side side, Quantity quantity) {
    open_orders_.insert_or_assign(id, OpenOrder{account, side, quantity});
    WorkingExposure& exposure = working_[account];
    if (side == Side::Buy) {
      exposure.buy += quantity.value();
    } else {
      exposure.sell += quantity.value();
    }
  }

  void reduce_open(OrderId id, Quantity fill) {
    const auto it = open_orders_.find(id);
    if (it == open_orders_.end()) {
      return;
    }

    OpenOrder& open = it->second;
    WorkingExposure& exposure = working_[open.account];
    if (open.side == Side::Buy) {
      exposure.buy -= fill.value();
    } else {
      exposure.sell -= fill.value();
    }

    open.remaining = open.remaining - fill;
    if (open.remaining.is_zero()) {
      open_orders_.erase(it);
    }
  }

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
  std::map<OrderId, OpenOrder> open_orders_;
  std::map<AccountId, WorkingExposure> working_;
};

}  // namespace mercury
