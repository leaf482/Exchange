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
  PostOnly,           // would take liquidity
  ReduceOnly,         // would open, increase, or flip position
  InsufficientCash,   // buy notional exceeds cash (when enforced)
};

struct RiskLimits {
  // 0 means unlimited.
  Quantity max_order_quantity{0};
  std::uint64_t max_abs_position{0};
};

// working_buy / working_sell are resting open quantities for the account+symbol.
inline RiskDecision check_order(const RiskLimits& limits,
                                const Positions& positions,
                                AccountId account,
                                Side side,
                                Quantity quantity,
                                std::uint64_t working_buy = 0,
                                std::uint64_t working_sell = 0,
                                Symbol symbol = Symbol{0}) {
  if (quantity.is_zero()) {
    return RiskDecision::OrderTooLarge;
  }

  if (limits.max_order_quantity.value() != 0 &&
      quantity > limits.max_order_quantity) {
    return RiskDecision::OrderTooLarge;
  }

  if (limits.max_abs_position != 0) {
    const std::int64_t pos = positions.quantity(account, symbol);
    const std::int64_t qty = static_cast<std::int64_t>(quantity.value());
    const std::int64_t open_buy = static_cast<std::int64_t>(working_buy);
    const std::int64_t open_sell = static_cast<std::int64_t>(working_sell);

    const std::int64_t max_long =
        pos + open_buy + (side == Side::Buy ? qty : 0);
    const std::int64_t max_short =
        pos - open_sell - (side == Side::Sell ? qty : 0);

    if (static_cast<std::uint64_t>(std::abs(max_long)) > limits.max_abs_position ||
        static_cast<std::uint64_t>(std::abs(max_short)) > limits.max_abs_position) {
      return RiskDecision::PositionLimit;
    }
  }

  return RiskDecision::Accept;
}

// Buy only closes short; sell only closes long. Qty may not exceed |position|.
inline RiskDecision check_reduce_only(const Positions& positions,
                                      AccountId account,
                                      Side side,
                                      Quantity quantity,
                                      Symbol symbol = Symbol{0}) {
  const std::int64_t pos = positions.quantity(account, symbol);
  const std::int64_t qty = static_cast<std::int64_t>(quantity.value());
  if (side == Side::Buy) {
    if (pos >= 0 || qty > -pos) {
      return RiskDecision::ReduceOnly;
    }
  } else if (pos <= 0 || qty > pos) {
    return RiskDecision::ReduceOnly;
  }
  return RiskDecision::Accept;
}

}  // namespace mercury
