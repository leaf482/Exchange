#pragma once

#include "mercury/types.hpp"

namespace mercury {

struct Trade {
  OrderId maker_id;
  OrderId taker_id;
  AccountId maker_account{0};
  AccountId taker_account{0};
  Price price;
  Quantity quantity;
  Symbol symbol{0};
  // Signed fee in tick*qty units (positive = account pays). Set by Engine.
  std::int64_t maker_fee{0};
  std::int64_t taker_fee{0};

  constexpr bool operator==(const Trade&) const = default;
};

}  // namespace mercury
