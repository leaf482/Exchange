#pragma once

#include "mercury/types.hpp"

namespace mercury {

struct Trade {
  OrderId maker_id;
  OrderId taker_id;
  Price price;
  Quantity quantity;

  constexpr bool operator==(const Trade&) const = default;
};

}  // namespace mercury
