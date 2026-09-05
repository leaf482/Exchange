#pragma once

#include "mercury/event_log.hpp"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

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

inline const char* format_side(Side side) {
  return side == Side::Buy ? "buy" : "sell";
}

inline TimeInForce parse_tif(std::string_view tif) {
  if (tif == "gtc") {
    return TimeInForce::Gtc;
  }
  if (tif == "ioc") {
    return TimeInForce::Ioc;
  }
  if (tif == "fok") {
    return TimeInForce::Fok;
  }
  throw std::runtime_error("invalid tif");
}

inline const char* format_tif(TimeInForce tif) {
  switch (tif) {
    case TimeInForce::Gtc:
      return "gtc";
    case TimeInForce::Ioc:
      return "ioc";
    case TimeInForce::Fok:
      return "fok";
  }
  return "gtc";
}

inline TimeInForce optional_tif(std::string_view line) {
  const auto value = field(line, "tif");
  return value ? parse_tif(*value) : TimeInForce::Gtc;
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
        .tif = detail::optional_tif(line),
        .symbol = Symbol{static_cast<std::uint64_t>(
            detail::field(line, "symbol") ? detail::require_int(line, "symbol") : 0)},
    };
  }
  if (type == "market") {
    return MarketOrder{
        .id = OrderId{static_cast<std::uint64_t>(detail::require_int(line, "id"))},
        .side = detail::parse_side(detail::require_string(line, "side")),
        .quantity = Quantity{static_cast<std::uint64_t>(detail::require_int(line, "quantity"))},
        .account = AccountId{static_cast<std::uint64_t>(
            detail::field(line, "account") ? detail::require_int(line, "account") : 0)},
        .symbol = Symbol{static_cast<std::uint64_t>(
            detail::field(line, "symbol") ? detail::require_int(line, "symbol") : 0)},
    };
  }
  if (type == "cancel") {
    return CancelOrder{
        .id = OrderId{static_cast<std::uint64_t>(detail::require_int(line, "id"))},
    };
  }
  throw std::runtime_error("unknown event type: " + type);
}

inline std::string format_event_line(const Event& event) {
  return std::visit(
      [](const auto& payload) -> std::string {
        using T = std::decay_t<decltype(payload)>;
        std::ostringstream out;
        if constexpr (std::is_same_v<T, Order>) {
          out << "{\"type\":\"limit\""
              << ",\"id\":" << payload.id.value()
              << ",\"side\":\"" << detail::format_side(payload.side) << '"'
              << ",\"price\":" << payload.price.ticks()
              << ",\"quantity\":" << payload.quantity.value()
              << ",\"account\":" << payload.account.value()
              << ",\"tif\":\"" << detail::format_tif(payload.tif) << '"'
              << ",\"symbol\":" << payload.symbol.value() << '}';
        } else if constexpr (std::is_same_v<T, MarketOrder>) {
          out << "{\"type\":\"market\""
              << ",\"id\":" << payload.id.value()
              << ",\"side\":\"" << detail::format_side(payload.side) << '"'
              << ",\"quantity\":" << payload.quantity.value()
              << ",\"account\":" << payload.account.value()
              << ",\"symbol\":" << payload.symbol.value() << '}';
        } else {
          out << "{\"type\":\"cancel\",\"id\":" << payload.id.value() << '}';
        }
        return out.str();
      },
      event);
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

inline EventLog load_event_log_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to open event log: " + path.string());
  }
  return load_event_log(input);
}

inline void save_event_log(std::ostream& output, const EventLog& log) {
  for (const Event& event : log.events()) {
    output << format_event_line(event) << '\n';
  }
}

inline void save_event_log_file(const std::filesystem::path& path, const EventLog& log) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    throw std::runtime_error("failed to write event log: " + path.string());
  }
  save_event_log(output, log);
}

}  // namespace jsonl
}  // namespace mercury
