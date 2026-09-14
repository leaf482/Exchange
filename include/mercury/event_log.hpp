#pragma once

#include "mercury/engine.hpp"
#include "mercury/order_book.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace mercury {

struct CancelOrder {
  OrderId id;

  constexpr bool operator==(const CancelOrder&) const = default;
};

struct ReplaceOrder {
  OrderId id;
  Price price;
  Quantity quantity;

  constexpr bool operator==(const ReplaceOrder&) const = default;
};

struct MassCancelOrder {
  MassCancelFilter filter{};

  constexpr bool operator==(const MassCancelOrder&) const = default;
};

// Audit/session record of a rejected submit. Replay is a no-op.
struct RejectEvent {
  RiskDecision decision{RiskDecision::Accept};
  std::variant<Order, MarketOrder, StopOrder> attempt;

  bool operator==(const RejectEvent& other) const {
    return decision == other.decision && attempt == other.attempt;
  }
};

inline RejectEvent make_reject(RiskDecision decision, Order order) {
  return RejectEvent{.decision = decision, .attempt = std::move(order)};
}

inline RejectEvent make_reject(RiskDecision decision, MarketOrder order) {
  return RejectEvent{.decision = decision, .attempt = std::move(order)};
}

inline RejectEvent make_reject(RiskDecision decision, StopOrder order) {
  return RejectEvent{.decision = decision, .attempt = std::move(order)};
}

// Discrete clock advance for GTD expiry (Engine-only).
struct TimeAdvance {
  std::uint64_t time{0};

  constexpr bool operator==(const TimeAdvance&) const = default;
};

using Event = std::variant<Order, MarketOrder, CancelOrder, StopOrder, ReplaceOrder,
                           MassCancelOrder, RejectEvent, TimeAdvance>;

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

// OrderBook cannot arm stops / mass-cancel by account. Prefer apply(Engine&).
inline std::vector<Trade> apply(OrderBook& book, const Event& event) {
  return std::visit(
      [&](const auto& payload) -> std::vector<Trade> {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, Order>) {
          return book.add(payload);
        } else if constexpr (std::is_same_v<T, MarketOrder>) {
          return book.add_market(payload);
        } else if constexpr (std::is_same_v<T, CancelOrder>) {
          book.cancel(payload.id);
          return {};
        } else if constexpr (std::is_same_v<T, ReplaceOrder>) {
          auto replaced = book.replace(payload.id, payload.price, payload.quantity);
          return replaced ? std::move(*replaced) : std::vector<Trade>{};
        } else if constexpr (std::is_same_v<T, MassCancelOrder>) {
          throw std::runtime_error("mass_cancel events require Engine replay");
        } else if constexpr (std::is_same_v<T, RejectEvent>) {
          return {};
        } else if constexpr (std::is_same_v<T, TimeAdvance>) {
          throw std::runtime_error("time events require Engine replay");
        } else {
          throw std::runtime_error("stop events require Engine replay");
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

inline SubmitResult apply(Engine& engine, const Event& event) {
  return std::visit(
      [&](const auto& payload) -> SubmitResult {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, Order>) {
          return engine.add(payload);
        } else if constexpr (std::is_same_v<T, MarketOrder>) {
          return engine.add_market(payload);
        } else if constexpr (std::is_same_v<T, CancelOrder>) {
          engine.cancel(payload.id);
          return SubmitResult{.decision = RiskDecision::Accept};
        } else if constexpr (std::is_same_v<T, ReplaceOrder>) {
          auto replaced = engine.replace(payload.id, payload.price, payload.quantity);
          return replaced ? std::move(*replaced)
                          : SubmitResult{.decision = RiskDecision::Accept};
        } else if constexpr (std::is_same_v<T, MassCancelOrder>) {
          engine.mass_cancel(payload.filter);
          return SubmitResult{.decision = RiskDecision::Accept};
        } else if constexpr (std::is_same_v<T, RejectEvent>) {
          return SubmitResult{.decision = RiskDecision::Accept};
        } else if constexpr (std::is_same_v<T, TimeAdvance>) {
          engine.advance_time(payload.time);
          return SubmitResult{.decision = RiskDecision::Accept};
        } else {
          return engine.add_stop(payload);
        }
      },
      event);
}

inline std::vector<Trade> replay(Engine& engine, const EventLog& log) {
  std::vector<Trade> trades;
  for (const Event& event : log.events()) {
    auto batch = apply(engine, event).trades;
    trades.insert(trades.end(), batch.begin(), batch.end());
  }
  return trades;
}

}  // namespace mercury
