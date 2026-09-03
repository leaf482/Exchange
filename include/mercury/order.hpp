#pragma once

#include "mercury/types.hpp"

namespace mercury {

struct Order {
  OrderId id;
  Side side;
  Price price;
  Quantity quantity;
  AccountId account{0};

  constexpr bool operator==(const Order&) const = default;
};

struct MarketOrder {
  OrderId id;
  Side side;
  Quantity quantity;
  AccountId account{0};

  constexpr bool operator==(const MarketOrder&) const = default;
};

}  // namespace mercury
