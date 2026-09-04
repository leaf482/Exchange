#pragma once

#include "mercury/event_log.hpp"

#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mercury {
namespace jsonl {
namespace detail {

inline std::string_view trim(std::string_view text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
    text.remove_prefix(1);
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
    text.remove_suffix(1);
  }
  return text;
}

inline std::optional<std::string_view> field(std::string_view line, std::string_view key) {
  const std::string pattern = "\"" + std::string(key) + "\":";
  const auto pos = line.find(pattern);
  if (pos == std::string_view::npos) {
    return std::nullopt;
  }

  std::string_view value = line.substr(pos + pattern.size());
  value = trim(value);
  if (value.empty()) {
    return std::nullopt;
  }

  if (value.front() == '"') {
    const auto end = value.find('"', 1);
    if (end == std::string_view::npos) {
      return std::nullopt;
    }
    return value.substr(1, end - 1);
  }

  std::size_t end = 0;
  while (end < value.size() &&
         (std::isdigit(static_cast<unsigned char>(value[end])) || value[end] == '-')) {
    ++end;
  }
  return value.substr(0, end);
}

inline std::int64_t require_int(std::string_view line, std::string_view key) {
  const auto value = field(line, key);
  if (!value) {
    throw std::runtime_error("missing int field: " + std::string(key));
  }
  return std::stoll(std::string(*value));
}

inline std::string require_string(std::string_view line, std::string_view key) {
  const auto value = field(line, key);
  if (!value) {
    throw std::runtime_error("missing string field: " + std::string(key));
  }
  return std::string(*value);
}

inline Side parse_side(std::string_view side) {
  if (side == "buy") {
    return Side::Buy;
  }
  if (side == "sell") {
    return Side::Sell;
  }
  throw std::runtime_error("invalid side");
}

}  // namespace detail

inline Event parse_event_line(std::string_view line) {
  line = detail::trim(line);
  if (line.empty()) {
    throw std::runtime_error("empty event line");
  }

  const std::string type = detail::require_string(line, "type");
  if (type == "limit") {
    return Order{
        .id = OrderId{static_cast<std::uint64_t>(detail::require_int(line, "id"))},
        .side = detail::parse_side(detail::require_string(line, "side")),
        .price = Price{detail::require_int(line, "price")},
        .quantity = Quantity{static_cast<std::uint64_t>(detail::require_int(line, "quantity"))},
        .account = AccountId{static_cast<std::uint64_t>(
            detail::field(line, "account") ? detail::require_int(line, "account") : 0)},
    };
  }
  if (type == "market") {
    return MarketOrder{
        .id = OrderId{static_cast<std::uint64_t>(detail::require_int(line, "id"))},
        .side = detail::parse_side(detail::require_string(line, "side")),
        .quantity = Quantity{static_cast<std::uint64_t>(detail::require_int(line, "quantity"))},
        .account = AccountId{static_cast<std::uint64_t>(
            detail::field(line, "account") ? detail::require_int(line, "account") : 0)},
    };
  }
  if (type == "cancel") {
    return CancelOrder{
        .id = OrderId{static_cast<std::uint64_t>(detail::require_int(line, "id"))},
    };
  }
  throw std::runtime_error("unknown event type: " + type);
}

inline EventLog load_event_log(std::istream& input) {
  EventLog log;
  std::string line;
  while (std::getline(input, line)) {
    if (detail::trim(line).empty()) {
      continue;
    }
    log.append(parse_event_line(line));
  }
  return log;
}

inline EventLog load_event_log(std::string_view text) {
  std::istringstream input{std::string(text)};
  return load_event_log(input);
}

}  // namespace jsonl
}  // namespace mercury
