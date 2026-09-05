#pragma once

#include "mercury/types.hpp"

#include <optional>

namespace mercury {

enum class TimeInForce : std::uint8_t { Gtc, Ioc, Fok };

struct Order {
  OrderId id;
  Side side;
  Price price;
  Quantity quantity;
  AccountId account{0};
  TimeInForce tif{TimeInForce::Gtc};
  Symbol symbol{0};

  constexpr bool operator==(const Order&) const = default;
};

struct MarketOrder {
  OrderId id;
  Side side;
  Quantity quantity;
  AccountId account{0};
  Symbol symbol{0};

  constexpr bool operator==(const MarketOrder&) const = default;
};

// Armed until last trade reaches stop_price, then becomes limit or market.
struct StopOrder {
  OrderId id;
  Side side;
  Price stop_price;
  Quantity quantity;
  AccountId account{0};
  std::optional<Price> limit_price;  // nullopt => market on trigger
  TimeInForce tif{TimeInForce::Gtc};  // used when limit_price is set
  Symbol symbol{0};

  constexpr bool operator==(const StopOrder&) const = default;
};

}  // namespace mercury
