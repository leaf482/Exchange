#pragma once

#include "mercury/fees.hpp"
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

enum class MarkSource : std::uint8_t { LastTrade, Mid };

struct MassCancelFilter {
  std::optional<AccountId> account;
  std::optional<Symbol> symbol;
  std::optional<Side> side;

  constexpr bool operator==(const MassCancelFilter&) const = default;
};

// Per-symbol OrderBooks + shared Positions, with risk checks and stop orders.
class Engine {
 public:
  explicit Engine(RiskLimits limits = {},
                  SelfTradePrevention stp = SelfTradePrevention::Off,
                  FeeSchedule fees = {})
      : limits_(limits), stp_(stp), fees_(fees) {}

  SubmitResult add(Order order) {
    const Symbol symbol = order.symbol;
    auto result = submit_limit(std::move(order));
    if (result.decision != RiskDecision::Accept) {
      return result;
    }
    append_trades(result.trades, drain_stops(symbol));
    return result;
  }

  SubmitResult add_market(MarketOrder order) {
    const Symbol symbol = order.symbol;
    auto result = submit_market(std::move(order));
    if (result.decision != RiskDecision::Accept) {
      return result;
    }
    append_trades(result.trades, drain_stops(symbol));
    return result;
  }

  // Arms a stop. Triggers when last trade crosses stop_price (buy: >=, sell: <=).
  SubmitResult add_stop(StopOrder stop) {
    const AccountId account = stop.account;
    const Symbol symbol = stop.symbol;
    const WorkingExposure exposure = working_for(account, symbol);
    const RiskDecision decision =
        check_order(limits_, positions_, account, stop.side, stop.quantity,
                    exposure.buy, exposure.sell, symbol);
    if (decision != RiskDecision::Accept) {
      return SubmitResult{.decision = decision};
    }

    if (is_triggered(stop)) {
      return fire_stop(std::move(stop));
    }

    add_open(stop.id, symbol, account, stop.side, stop.quantity);
    instrument(symbol).stops.push_back(std::move(stop));
    return SubmitResult{.decision = RiskDecision::Accept};
  }

  bool cancel(OrderId id) {
    const auto open_it = open_orders_.find(id);
    if (open_it == open_orders_.end()) {
      return false;
    }

    const Symbol symbol = open_it->second.symbol;
    Instrument& inst = instrument(symbol);

    if (erase_stop(inst, id)) {
      return true;
    }

    reduce_open(id, open_it->second.remaining);
    return inst.book.cancel(id);
  }

  // Cancel-replace a resting GTC order (not a pending stop). Loses time priority.
  // quantity 0 cancels only. Returns nullopt if id is missing or is a stop.
  std::optional<SubmitResult> replace(OrderId id, Price price, Quantity quantity) {
    const auto open_it = open_orders_.find(id);
    if (open_it == open_orders_.end()) {
      return std::nullopt;
    }

    const Symbol symbol = open_it->second.symbol;
    Instrument& inst = instrument(symbol);
    for (const StopOrder& stop : inst.stops) {
      if (stop.id == id) {
        return std::nullopt;
      }
    }

    const OpenOrder open = open_it->second;
    if (!cancel(id)) {
      return std::nullopt;
    }
    if (quantity.is_zero()) {
      return SubmitResult{.decision = RiskDecision::Accept};
    }

    return add(Order{
        .id = id,
        .side = open.side,
        .price = price,
        .quantity = quantity,
        .account = open.account,
        .tif = TimeInForce::Gtc,
        .symbol = symbol,
    });
  }

  // Cancel resting orders and pending stops matching all set filter fields.
  std::size_t mass_cancel(MassCancelFilter filter = {}) {
    std::vector<OrderId> ids;
    ids.reserve(open_orders_.size());
    for (const auto& [id, open] : open_orders_) {
      if (filter.account && open.account != *filter.account) {
        continue;
      }
      if (filter.symbol && open.symbol != *filter.symbol) {
        continue;
      }
      if (filter.side && open.side != *filter.side) {
        continue;
      }
      ids.push_back(id);
    }

    std::size_t cancelled = 0;
    for (const OrderId id : ids) {
      if (cancel(id)) {
        ++cancelled;
      }
    }
    return cancelled;
  }

  BookSnapshot snapshot(std::size_t max_levels, Symbol symbol = Symbol{0}) const {
    const Instrument* inst = find_instrument(symbol);
    return inst ? inst->book.snapshot(max_levels) : BookSnapshot{};
  }

  const OrderBook& book(Symbol symbol = Symbol{0}) const {
    const Instrument* inst = find_instrument(symbol);
    if (inst) {
      return inst->book;
    }
    static const OrderBook empty;
    return empty;
  }

  const Positions& positions() const { return positions_; }

  const FeeSchedule& fees() const { return fees_; }

  // Cumulative fees paid by account (positive = paid; negative = rebate received).
  std::int64_t fees_paid(AccountId account) const {
    const auto it = fees_paid_.find(account);
    return it == fees_paid_.end() ? 0 : it->second;
  }

  std::optional<Price> last_trade_price(Symbol symbol = Symbol{0}) const {
    const Instrument* inst = find_instrument(symbol);
    return inst ? inst->last_trade_price : std::nullopt;
  }

  // Mid = (best_bid + best_ask) / 2 in ticks; nullopt if either side is missing.
  std::optional<Price> mid_price(Symbol symbol = Symbol{0}) const {
    const auto bid = book(symbol).best_bid();
    const auto ask = book(symbol).best_ask();
    if (!bid || !ask) {
      return std::nullopt;
    }
    return Price{(bid->ticks() + ask->ticks()) / 2};
  }

  std::optional<Price> mark_price(MarkSource source = MarkSource::LastTrade,
                                  Symbol symbol = Symbol{0}) const {
    switch (source) {
      case MarkSource::LastTrade:
        return last_trade_price(symbol);
      case MarkSource::Mid:
        return mid_price(symbol);
    }
    return std::nullopt;
  }

  std::int64_t realized_pnl(AccountId account, Symbol symbol = Symbol{0}) const {
    return positions_.realized_pnl(account, symbol);
  }

  // Default mark is last trade; Mid needs a two-sided book.
  std::optional<std::int64_t> unrealized_pnl(
      AccountId account, Symbol symbol = Symbol{0},
      MarkSource mark = MarkSource::LastTrade) const {
    const auto price = mark_price(mark, symbol);
    if (!price) {
      return std::nullopt;
    }
    return positions_.unrealized_pnl(account, *price, symbol);
  }

  std::size_t pending_stop_count(Symbol symbol = Symbol{0}) const {
    const Instrument* inst = find_instrument(symbol);
    return inst ? inst->stops.size() : 0;
  }

  // Next id that will be assigned (starts at 1).
  TradeId next_trade_id() const { return TradeId{next_trade_id_}; }

 private:
  struct WorkingExposure {
    std::uint64_t buy = 0;
    std::uint64_t sell = 0;
  };

  struct OpenOrder {
    Symbol symbol;
    AccountId account;
    Side side;
    Quantity remaining;
  };

  struct Instrument {
    explicit Instrument(SelfTradePrevention stp = SelfTradePrevention::Off)
        : book(stp) {}

    OrderBook book;
    std::vector<StopOrder> stops;
    std::optional<Price> last_trade_price;
  };

  Instrument& instrument(Symbol symbol) {
    const auto [it, inserted] = instruments_.try_emplace(symbol, stp_);
    (void)inserted;
    return it->second;
  }

  const Instrument* find_instrument(Symbol symbol) const {
    const auto it = instruments_.find(symbol);
    return it == instruments_.end() ? nullptr : &it->second;
  }

  WorkingExposure working_for(AccountId account, Symbol symbol) const {
    const auto it = working_.find({account, symbol});
    return it == working_.end() ? WorkingExposure{} : it->second;
  }

  void add_open(OrderId id, Symbol symbol, AccountId account, Side side,
                Quantity quantity) {
    open_orders_.insert_or_assign(id, OpenOrder{symbol, account, side, quantity});
    WorkingExposure& exposure = working_[{account, symbol}];
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
    WorkingExposure& exposure = working_[{open.account, open.symbol}];
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

  void apply_trades(Symbol symbol, Side taker_side, std::vector<Trade>& trades) {
    Instrument& inst = instrument(symbol);
    const Side maker_side = opposite_side(taker_side);
    for (Trade& trade : trades) {
      trade.id = TradeId{next_trade_id_++};
      const std::int64_t notional =
          trade.price.ticks() * static_cast<std::int64_t>(trade.quantity.value());
      trade.maker_fee = fee_from_notional(notional, fees_.maker_bps);
      trade.taker_fee = fee_from_notional(notional, fees_.taker_bps);
      fees_paid_[trade.maker_account] += trade.maker_fee;
      fees_paid_[trade.taker_account] += trade.taker_fee;

      positions_.fill(trade.taker_account, taker_side, trade.price, trade.quantity,
                      symbol);
      positions_.fill(trade.maker_account, maker_side, trade.price, trade.quantity,
                      symbol);
      inst.last_trade_price = trade.price;
    }
  }

  static void append_trades(std::vector<Trade>& into, std::vector<Trade> extra) {
    into.insert(into.end(), extra.begin(), extra.end());
  }

  bool is_triggered(const StopOrder& stop) const {
    const Instrument* inst = find_instrument(stop.symbol);
    if (!inst || !inst->last_trade_price) {
      return false;
    }
    if (stop.side == Side::Buy) {
      return inst->last_trade_price->ticks() >= stop.stop_price.ticks();
    }
    return inst->last_trade_price->ticks() <= stop.stop_price.ticks();
  }

  bool erase_stop(Instrument& inst, OrderId id) {
    for (auto it = inst.stops.begin(); it != inst.stops.end(); ++it) {
      if (it->id == id) {
        reduce_open(id, it->quantity);
        inst.stops.erase(it);
        return true;
      }
    }
    return false;
  }

  SubmitResult fire_stop(StopOrder stop) {
    SubmitResult result;
    const Symbol symbol = stop.symbol;
    if (stop.limit_price) {
      result = submit_limit(Order{
          .id = stop.id,
          .side = stop.side,
          .price = *stop.limit_price,
          .quantity = stop.quantity,
          .account = stop.account,
          .tif = stop.tif,
          .symbol = symbol,
      });
    } else {
      result = submit_market(MarketOrder{
          .id = stop.id,
          .side = stop.side,
          .quantity = stop.quantity,
          .account = stop.account,
          .symbol = symbol,
      });
    }
    if (result.decision == RiskDecision::Accept) {
      append_trades(result.trades, drain_stops(symbol));
    }
    return result;
  }

  std::vector<Trade> drain_stops(Symbol symbol) {
    std::vector<Trade> trades;
    Instrument& inst = instrument(symbol);
    bool progressed = true;
    while (progressed) {
      progressed = false;
      for (auto it = inst.stops.begin(); it != inst.stops.end();) {
        if (!is_triggered(*it)) {
          ++it;
          continue;
        }

        StopOrder stop = std::move(*it);
        it = inst.stops.erase(it);
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
              .symbol = symbol,
          });
        } else {
          fired = submit_market(MarketOrder{
              .id = stop.id,
              .side = stop.side,
              .quantity = stop.quantity,
              .account = stop.account,
              .symbol = symbol,
          });
        }

        if (fired.decision == RiskDecision::Accept) {
          append_trades(trades, std::move(fired.trades));
          progressed = true;
        }
        break;  // restart scan; vector invalidated / price may have moved
      }
    }
    return trades;
  }

  SubmitResult submit_limit(Order order) {
    const AccountId account = order.account;
    const Symbol symbol = order.symbol;
    const WorkingExposure exposure = working_for(account, symbol);
    const RiskDecision decision =
        check_order(limits_, positions_, account, order.side, order.quantity,
                    exposure.buy, exposure.sell, symbol);
    if (decision != RiskDecision::Accept) {
      return SubmitResult{.decision = decision};
    }
    if (order.reduce_only) {
      const RiskDecision reduce =
          check_reduce_only(positions_, account, order.side, order.quantity, symbol);
      if (reduce != RiskDecision::Accept) {
        return SubmitResult{.decision = reduce};
      }
    }
    if (order.post_only && instrument(symbol).book.would_take(order)) {
      return SubmitResult{.decision = RiskDecision::PostOnly};
    }

    const OrderId id = order.id;
    const Side taker_side = order.side;
    const Quantity original = order.quantity;
    const TimeInForce tif = order.tif;
    auto trades = instrument(symbol).book.add(std::move(order));
    clear_stp_cancels(symbol);

    Quantity filled{0};
    for (const Trade& trade : trades) {
      filled = Quantity{filled.value() + trade.quantity.value()};
      reduce_open(trade.maker_id, trade.quantity);
    }

    const Quantity rested{original.value() - filled.value()};
    if (tif == TimeInForce::Gtc && !rested.is_zero()) {
      add_open(id, symbol, account, taker_side, rested);
    }

    apply_trades(symbol, taker_side, trades);
    return SubmitResult{.decision = RiskDecision::Accept, .trades = std::move(trades)};
  }

  SubmitResult submit_market(MarketOrder order) {
    const AccountId account = order.account;
    const Symbol symbol = order.symbol;
    const WorkingExposure exposure = working_for(account, symbol);
    const RiskDecision decision =
        check_order(limits_, positions_, account, order.side, order.quantity,
                    exposure.buy, exposure.sell, symbol);
    if (decision != RiskDecision::Accept) {
      return SubmitResult{.decision = decision};
    }
    if (order.reduce_only) {
      const RiskDecision reduce =
          check_reduce_only(positions_, account, order.side, order.quantity, symbol);
      if (reduce != RiskDecision::Accept) {
        return SubmitResult{.decision = reduce};
      }
    }

    const Side taker_side = order.side;
    auto trades = instrument(symbol).book.add_market(std::move(order));
    clear_stp_cancels(symbol);
    for (const Trade& trade : trades) {
      reduce_open(trade.maker_id, trade.quantity);
    }
    apply_trades(symbol, taker_side, trades);
    return SubmitResult{.decision = RiskDecision::Accept, .trades = std::move(trades)};
  }

  void clear_stp_cancels(Symbol symbol) {
    auto cancels = instrument(symbol).book.take_stp_cancels();
    for (const OrderId id : cancels) {
      const auto it = open_orders_.find(id);
      if (it != open_orders_.end()) {
        reduce_open(id, it->second.remaining);
      }
    }
  }

  RiskLimits limits_;
  SelfTradePrevention stp_;
  FeeSchedule fees_;
  std::map<Symbol, Instrument> instruments_;
  Positions positions_;
  std::map<OrderId, OpenOrder> open_orders_;
  std::map<std::pair<AccountId, Symbol>, WorkingExposure> working_;
  std::map<AccountId, std::int64_t> fees_paid_;
  std::uint64_t next_trade_id_{1};
};

}  // namespace mercury
