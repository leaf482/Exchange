#pragma once

#include "mercury/types.hpp"

#include <algorithm>
#include <optional>

namespace mercury {

enum class TimeInForce : std::uint8_t { Gtc, Ioc, Fok, Gtd };

inline bool rests_on_book(TimeInForce tif) {
  return tif == TimeInForce::Gtc || tif == TimeInForce::Gtd;
}

// When enabled, a taker does not trade against its own resting orders.
// CancelResting: drop the resting order and keep matching.
enum class SelfTradePrevention : std::uint8_t { Off, CancelResting };

struct Order {
  OrderId id;
  Side side;
  Price price;
  Quantity quantity;
  AccountId account{0};
  TimeInForce tif{TimeInForce::Gtc};
  Symbol symbol{0};
  bool post_only{false};   // reject if the order would take liquidity
  bool reduce_only{false};  // reject unless it shrinks existing position (no flip)
  // Iceberg peak: 0 = fully visible. Snapshot shows min(quantity, display);
  // matching still uses full quantity.
  Quantity display{0};
  // Discrete engine clock deadline; required when tif == Gtd (must be > now).
  std::uint64_t expire_at{0};

  constexpr bool operator==(const Order&) const = default;
};

inline Quantity visible_quantity(const Order& order) {
  if (order.display.is_zero()) {
    return order.quantity;
  }
  return std::min(order.quantity, order.display);
}

struct MarketOrder {
  OrderId id;
  Side side;
  Quantity quantity;
  AccountId account{0};
  Symbol symbol{0};
  bool reduce_only{false};

  constexpr bool operator==(const MarketOrder&) const = default;
};

// Armed until last trade reaches stop_price, then becomes limit or market.
// Optional expire_at cancels the pending stop when the Engine clock reaches it.
struct StopOrder {
  OrderId id;
  Side side;
  Price stop_price;
  Quantity quantity;
  AccountId account{0};
  std::optional<Price> limit_price;  // nullopt => market on trigger
  TimeInForce tif{TimeInForce::Gtc};  // used when limit_price is set
  Symbol symbol{0};
  std::uint64_t expire_at{0};  // 0 = no pending expiry

  constexpr bool operator==(const StopOrder&) const = default;
};

}  // namespace mercury
