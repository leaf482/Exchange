#pragma once

#include "mercury/price_level.hpp"

#include <functional>
#include <map>
#include <optional>
#include <utility>

namespace mercury {

class OrderBook {
 public:
  void add(Order order) {
    if (order.side == Side::Buy) {
      auto [it, _] = bids_.try_emplace(order.price, order.price);
      it->second.enqueue(std::move(order));
      return;
    }

    auto [it, _] = asks_.try_emplace(order.price, order.price);
    it->second.enqueue(std::move(order));
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
  // Best bid = highest price, best ask = lowest price.
  std::map<Price, PriceLevel, std::greater<>> bids_;
  std::map<Price, PriceLevel> asks_;
};

}  // namespace mercury
