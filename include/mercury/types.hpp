#pragma once

#include <cstdint>
#include <compare>

namespace mercury {

enum class Side : std::uint8_t { Buy, Sell };

class OrderId {
 public:
  constexpr explicit OrderId(std::uint64_t value) noexcept : value_(value) {}

  constexpr std::uint64_t value() const noexcept { return value_; }

  constexpr auto operator<=>(const OrderId&) const = default;

 private:
  std::uint64_t value_;
};

// Price in integer ticks (smallest price increment). Never use floating-point.
class Price {
 public:
  constexpr explicit Price(std::int64_t ticks) noexcept : ticks_(ticks) {}

  constexpr std::int64_t ticks() const noexcept { return ticks_; }

  constexpr auto operator<=>(const Price&) const = default;

 private:
  std::int64_t ticks_;
};

class Quantity {
 public:
  constexpr explicit Quantity(std::uint64_t value) noexcept : value_(value) {}

  constexpr std::uint64_t value() const noexcept { return value_; }

  constexpr auto operator<=>(const Quantity&) const = default;

 private:
  std::uint64_t value_;
};

}  // namespace mercury
