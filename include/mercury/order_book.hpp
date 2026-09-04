#pragma once

#include "mercury/price_level.hpp"
#include "mercury/trade.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace mercury {

struct BookLevel {
  Price price;
  Quantity quantity;
  std::size_t order_count = 0;

  constexpr bool operator==(const BookLevel&) const = default;
};

struct BookSnapshot {
  std::vector<BookLevel> bids;  // best bid first
  std::vector<BookLevel> asks;  // best ask first

  std::optional<Price> best_bid() const {
    if (bids.empty()) {
      return std::nullopt;
    }
    return bids.front().price;
  }

  std::optional<Price> best_ask() const {
    if (asks.empty()) {
      return std::nullopt;
    }
    return asks.front().price;
  }

  std::optional<std::int64_t> spread_ticks() const {
    const auto bid = best_bid();
    const auto ask = best_ask();
    if (!bid || !ask) {
      return std::nullopt;
    }
    return ask->ticks() - bid->ticks();
  }
};

class OrderBook {
 public:
  // Match against the opposite side, then rest any remainder.
  std::vector<Trade> add(Order order) {
    std::vector<Trade> trades;

    if (order.side == Side::Buy) {
      match_buy(order, trades, false);
    } else {
      match_sell(order, trades, false);
    }

    if (!order.quantity.is_zero()) {
      rest(std::move(order));
    }

    return trades;
  }

  // Fill against available liquidity; any unfilled quantity is discarded.
  std::vector<Trade> add_market(MarketOrder order) {
    Order taker{
        .id = order.id,
        .side = order.side,
        .price = Price{0},
        .quantity = order.quantity,
        .account = order.account,
    };

    std::vector<Trade> trades;
    if (taker.side == Side::Buy) {
      match_buy(taker, trades, true);
    } else {
      match_sell(taker, trades, true);
    }
    return trades;
  }

  // Removes a resting order. Returns false if the id is not on the book.
  bool cancel(OrderId id) {
    const auto loc_it = index_.find(id);
    if (loc_it == index_.end()) {
      return false;
    }

    const RestingLocation loc = loc_it->second;
    if (loc.side == Side::Buy) {
      erase_from(bids_, loc.price, id);
    } else {
      erase_from(asks_, loc.price, id);
    }

    index_.erase(loc_it);
    return true;
  }

  std::optional<Price> best_bid() const {
    if (bids_.empty()) {
      return std::nullopt;
    }
    return bids_.begin()->first;
  }

  std::optional<Price> best_ask() const {
    if (asks_.empty()) {
      return std::nullopt;
    }
    return asks_.begin()->first;
  }

  BookSnapshot snapshot(std::size_t max_levels) const {
    BookSnapshot snap;
    for (const auto& [price, level] : bids_) {
      if (snap.bids.size() >= max_levels) {
        break;
      }
      snap.bids.push_back(BookLevel{
          .price = price,
          .quantity = level.total_quantity(),
          .order_count = level.size(),
      });
    }
    for (const auto& [price, level] : asks_) {
      if (snap.asks.size() >= max_levels) {
        break;
      }
      snap.asks.push_back(BookLevel{
          .price = price,
          .quantity = level.total_quantity(),
          .order_count = level.size(),
      });
    }
    return snap;
  }

 private:
  struct RestingLocation {
    Side side;
    Price price;
  };

  using BidLevels = std::map<Price, PriceLevel, std::greater<>>;
  using AskLevels = std::map<Price, PriceLevel>;

  void rest(Order order) {
    const OrderId id = order.id;
    const RestingLocation loc{order.side, order.price};

    if (order.side == Side::Buy) {
      auto [it, _] = bids_.try_emplace(order.price, order.price);
      it->second.enqueue(std::move(order));
    } else {
      auto [it, _] = asks_.try_emplace(order.price, order.price);
      it->second.enqueue(std::move(order));
    }

    index_.emplace(id, loc);
  }

  void match_buy(Order& taker, std::vector<Trade>& trades, bool is_market) {
    while (!taker.quantity.is_zero() && !asks_.empty()) {
      auto level_it = asks_.begin();
      if (!is_market && level_it->first > taker.price) {
        break;
      }
      fill_level(level_it->second, taker, trades);
      if (level_it->second.empty()) {
        asks_.erase(level_it);
      }
    }
  }

  void match_sell(Order& taker, std::vector<Trade>& trades, bool is_market) {
    while (!taker.quantity.is_zero() && !bids_.empty()) {
      auto level_it = bids_.begin();
      if (!is_market && level_it->first < taker.price) {
        break;
      }
      fill_level(level_it->second, taker, trades);
      if (level_it->second.empty()) {
        bids_.erase(level_it);
      }
    }
  }

  void fill_level(PriceLevel& level, Order& taker, std::vector<Trade>& trades) {
    while (!taker.quantity.is_zero() && !level.empty()) {
      Order& maker = level.front();
      const Quantity fill = std::min(taker.quantity, maker.quantity);

      trades.push_back(Trade{
          .maker_id = maker.id,
          .taker_id = taker.id,
          .maker_account = maker.account,
          .taker_account = taker.account,
          .price = maker.price,
          .quantity = fill,
      });

      taker.quantity = taker.quantity - fill;
      maker.quantity = maker.quantity - fill;

      if (maker.quantity.is_zero()) {
        index_.erase(maker.id);
        level.dequeue();
      }
    }
  }

  template <typename Levels>
  static void erase_from(Levels& levels, Price price, OrderId id) {
    const auto level_it = levels.find(price);
    assert(level_it != levels.end());
    [[maybe_unused]] const bool erased = level_it->second.erase(id);
    assert(erased);
    if (level_it->second.empty()) {
      levels.erase(level_it);
    }
  }

  // Best bid = highest price, best ask = lowest price.
  BidLevels bids_;
  AskLevels asks_;
  std::map<OrderId, RestingLocation> index_;
};

}  // namespace mercury
