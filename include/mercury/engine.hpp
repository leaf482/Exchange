#pragma once

#include "mercury/order_book.hpp"
#include "mercury/positions.hpp"
#include "mercury/risk.hpp"

#include <cstdint>
#include <map>
#include <optional>
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

// OrderBook + Positions with optional pre-trade risk checks and stop orders.
class Engine {
 public:
  explicit Engine(RiskLimits limits = {}) : limits_(limits) {}

  SubmitResult add(Order order) {
    auto result = submit_limit(std::move(order));
    if (result.decision != RiskDecision::Accept) {
      return result;
    }
    append_trades(result.trades, drain_stops());
    return result;
  }

  SubmitResult add_market(MarketOrder order) {
    auto result = submit_market(std::move(order));
    if (result.decision != RiskDecision::Accept) {
      return result;
    }
    append_trades(result.trades, drain_stops());
    return result;
  }

  // Arms a stop. Triggers when last trade crosses stop_price (buy: >=, sell: <=).
  SubmitResult add_stop(StopOrder stop) {
    const AccountId account = stop.account;
    const WorkingExposure exposure = working_for(account);
    const RiskDecision decision = check_order(limits_, positions_, account, stop.side,
                                              stop.quantity, exposure.buy, exposure.sell);
    if (decision != RiskDecision::Accept) {
      return SubmitResult{.decision = decision};
    }

    if (is_triggered(stop)) {
      return fire_stop(std::move(stop));
    }

    add_open(stop.id, account, stop.side, stop.quantity);
    stops_.push_back(std::move(stop));
    return SubmitResult{.decision = RiskDecision::Accept};
  }

  bool cancel(OrderId id) {
    if (erase_stop(id)) {
      return true;
    }

    const auto open_it = open_orders_.find(id);
    if (open_it != open_orders_.end()) {
      reduce_open(id, open_it->second.remaining);
    }
    return book_.cancel(id);
  }

  BookSnapshot snapshot(std::size_t max_levels) const { return book_.snapshot(max_levels); }

  const OrderBook& book() const { return book_; }

  const Positions& positions() const { return positions_; }

  std::optional<Price> last_trade_price() const { return last_trade_price_; }

  std::size_t pending_stop_count() const { return stops_.size(); }

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
      last_trade_price_ = trade.price;
    }
  }

  static void append_trades(std::vector<Trade>& into, std::vector<Trade> extra) {
    into.insert(into.end(), extra.begin(), extra.end());
  }

  bool is_triggered(const StopOrder& stop) const {
    if (!last_trade_price_) {
      return false;
    }
    if (stop.side == Side::Buy) {
      return last_trade_price_->ticks() >= stop.stop_price.ticks();
    }
    return last_trade_price_->ticks() <= stop.stop_price.ticks();
  }

  bool erase_stop(OrderId id) {
    for (auto it = stops_.begin(); it != stops_.end(); ++it) {
      if (it->id == id) {
        reduce_open(id, it->quantity);
        stops_.erase(it);
        return true;
      }
    }
    return false;
  }

  SubmitResult fire_stop(StopOrder stop) {
    SubmitResult result;
    if (stop.limit_price) {
      result = submit_limit(Order{
          .id = stop.id,
          .side = stop.side,
          .price = *stop.limit_price,
          .quantity = stop.quantity,
          .account = stop.account,
          .tif = stop.tif,
      });
    } else {
      result = submit_market(MarketOrder{
          .id = stop.id,
          .side = stop.side,
          .quantity = stop.quantity,
          .account = stop.account,
      });
    }
    if (result.decision == RiskDecision::Accept) {
      append_trades(result.trades, drain_stops());
    }
    return result;
  }

  std::vector<Trade> drain_stops() {
    std::vector<Trade> trades;
    bool progressed = true;
    while (progressed) {
      progressed = false;
      for (auto it = stops_.begin(); it != stops_.end();) {
        if (!is_triggered(*it)) {
          ++it;
          continue;
        }

        StopOrder stop = std::move(*it);
        it = stops_.erase(it);
        reduce_open(stop.id, stop.quantity);

        SubmitResult fired;
        if (stop.limit_price) {
          fired = submit_limit(Order{
              .id = stop.id,
              .side = stop.side,
              .price = *stop.limit_price,
              .quantity = stop.quantity,
              .account = stop.account,
              .tif = stop.tif,
          });
        } else {
          fired = submit_market(MarketOrder{
              .id = stop.id,
              .side = stop.side,
              .quantity = stop.quantity,
              .account = stop.account,
          });
        }

        if (fired.decision == RiskDecision::Accept) {
          append_trades(trades, std::move(fired.trades));
          progressed = true;
        }
        // Rejected stop is dropped (already removed from pending).
        break;  // restart scan; vector invalidated / price may have moved
      }
    }
    return trades;
  }

  SubmitResult submit_limit(Order order) {
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
    const TimeInForce tif = order.tif;
    auto trades = book_.add(std::move(order));

    Quantity filled{0};
    for (const Trade& trade : trades) {
      filled = Quantity{filled.value() + trade.quantity.value()};
      reduce_open(trade.maker_id, trade.quantity);
    }

    const Quantity rested{original.value() - filled.value()};
    if (tif == TimeInForce::Gtc && !rested.is_zero()) {
      add_open(id, account, taker_side, rested);
    }

    apply_trades(taker_side, trades);
    return SubmitResult{.decision = RiskDecision::Accept, .trades = std::move(trades)};
  }

  SubmitResult submit_market(MarketOrder order) {
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

  RiskLimits limits_;
  OrderBook book_;
  Positions positions_;
  std::map<OrderId, OpenOrder> open_orders_;
  std::map<AccountId, WorkingExposure> working_;
  std::vector<StopOrder> stops_;
  std::optional<Price> last_trade_price_;
};

}  // namespace mercury
