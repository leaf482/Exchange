#pragma once

#include "mercury/price_level.hpp"
#include "mercury/trade.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace mercury {

class OrderBook {
 public:
  // Match against the opposite side, then rest any remainder.
  std::vector<Trade> add(Order order) {
    std::vector<Trade> trades;

    if (order.side == Side::Buy) {
      match_buy(order, trades);
    } else {
      match_sell(order, trades);
    }

    if (!order.quantity.is_zero()) {
      rest(std::move(order));
    }

    return trades;
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

 private:
  using BidLevels = std::map<Price, PriceLevel, std::greater<>>;
  using AskLevels = std::map<Price, PriceLevel>;

  void rest(Order order) {
    if (order.side == Side::Buy) {
      auto [it, _] = bids_.try_emplace(order.price, order.price);
      it->second.enqueue(std::move(order));
      return;
    }

    auto [it, _] = asks_.try_emplace(order.price, order.price);
    it->second.enqueue(std::move(order));
  }

  void match_buy(Order& taker, std::vector<Trade>& trades) {
    while (!taker.quantity.is_zero() && !asks_.empty()) {
      auto level_it = asks_.begin();
      if (level_it->first > taker.price) {
        break;
      }
      fill_level(level_it->second, taker, trades);
      if (level_it->second.empty()) {
        asks_.erase(level_it);
      }
    }
  }

  void match_sell(Order& taker, std::vector<Trade>& trades) {
    while (!taker.quantity.is_zero() && !bids_.empty()) {
      auto level_it = bids_.begin();
      if (level_it->first < taker.price) {
        break;
      }
      fill_level(level_it->second, taker, trades);
      if (level_it->second.empty()) {
        bids_.erase(level_it);
      }
    }
  }

  static void fill_level(PriceLevel& level, Order& taker, std::vector<Trade>& trades) {
    while (!taker.quantity.is_zero() && !level.empty()) {
      Order& maker = level.front();
      const Quantity fill = std::min(taker.quantity, maker.quantity);

      trades.push_back(Trade{
          .maker_id = maker.id,
          .taker_id = taker.id,
          .price = maker.price,
          .quantity = fill,
      });

      taker.quantity = taker.quantity - fill;
      maker.quantity = maker.quantity - fill;

      if (maker.quantity.is_zero()) {
        level.dequeue();
      }
    }
  }

  // Best bid = highest price, best ask = lowest price.
  BidLevels bids_;
  AskLevels asks_;
};

}  // namespace mercury
