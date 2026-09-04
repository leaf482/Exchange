#pragma once

#include "mercury/types.hpp"

namespace mercury {

enum class TimeInForce : std::uint8_t { Gtc, Ioc, Fok };

struct Order {
  OrderId id;
  Side side;
  Price price;
  Quantity quantity;
  AccountId account{0};
  TimeInForce tif{TimeInForce::Gtc};

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
