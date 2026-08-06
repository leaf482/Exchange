#pragma once

#include "mercury/order_book.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace mercury {

struct CancelOrder {
  OrderId id;

  constexpr bool operator==(const CancelOrder&) const = default;
};

using Event = std::variant<Order, MarketOrder, CancelOrder>;

class EventLog {
 public:
  void append(Event event) { events_.push_back(std::move(event)); }

  bool empty() const { return events_.empty(); }

  std::size_t size() const { return events_.size(); }

  const Event& at(std::size_t index) const { return events_.at(index); }

  const std::vector<Event>& events() const { return events_; }

 private:
  std::vector<Event> events_;
};

inline std::vector<Trade> apply(OrderBook& book, const Event& event) {
  return std::visit(
      [&](const auto& payload) -> std::vector<Trade> {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, Order>) {
          return book.add(payload);
        } else if constexpr (std::is_same_v<T, MarketOrder>) {
          return book.add_market(payload);
        } else {
          book.cancel(payload.id);
          return {};
        }
      },
      event);
}

inline std::vector<Trade> replay(OrderBook& book, const EventLog& log) {
  std::vector<Trade> trades;
  for (const Event& event : log.events()) {
    auto batch = apply(book, event);
    trades.insert(trades.end(), batch.begin(), batch.end());
  }
  return trades;
}

}  // namespace mercury
