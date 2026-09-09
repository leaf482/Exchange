#pragma once

#include "mercury/types.hpp"

#include <cstdint>

namespace mercury {

// Fees in basis points of notional (price_ticks * quantity).
// Positive = pay; negative = rebate. Applied per fill in tick*qty units.
struct FeeSchedule {
  std::int64_t maker_bps{0};
  std::int64_t taker_bps{0};

  constexpr bool operator==(const FeeSchedule&) const = default;
};

inline std::int64_t fee_from_notional(std::int64_t notional_ticks, std::int64_t bps) {
  return (notional_ticks * bps) / 10'000;
}

}  // namespace mercury
