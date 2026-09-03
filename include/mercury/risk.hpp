#pragma once

#include "mercury/positions.hpp"
#include "mercury/types.hpp"

#include <cstdint>
#include <cstdlib>

namespace mercury {

enum class RiskDecision : std::uint8_t {
  Accept,
  OrderTooLarge,
  PositionLimit,
};

struct RiskLimits {
  // 0 means unlimited.
  Quantity max_order_quantity{0};
  std::uint64_t max_abs_position{0};
};

inline RiskDecision check_order(const RiskLimits& limits,
                                const Positions& positions,
                                AccountId account,
                                Side side,
                                Quantity quantity) {
  if (quantity.is_zero()) {
    return RiskDecision::OrderTooLarge;
  }

  if (limits.max_order_quantity.value() != 0 &&
      quantity > limits.max_order_quantity) {
    return RiskDecision::OrderTooLarge;
  }

  if (limits.max_abs_position != 0) {
    const std::int64_t pos = positions.quantity(account);
    const std::int64_t qty = static_cast<std::int64_t>(quantity.value());
    const std::int64_t projected = (side == Side::Buy) ? pos + qty : pos - qty;
    if (static_cast<std::uint64_t>(std::abs(projected)) > limits.max_abs_position) {
      return RiskDecision::PositionLimit;
    }
  }

  return RiskDecision::Accept;
}

}  // namespace mercury
