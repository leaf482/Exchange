#pragma once

#include "mercury/types.hpp"

#include <cstdint>
#include <map>

namespace mercury {

// Cash ledger in tick * quantity units (same as notional / fees).
class Balances {
 public:
  std::int64_t cash(AccountId account) const {
    const auto it = cash_.find(account);
    return it == cash_.end() ? 0 : it->second;
  }

  void set_cash(AccountId account, std::int64_t amount) { cash_[account] = amount; }

  void adjust(AccountId account, std::int64_t delta) { cash_[account] += delta; }

 private:
  std::map<AccountId, std::int64_t> cash_;
};

}  // namespace mercury
