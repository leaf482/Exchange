#include <mercury/engine.hpp>
#include <mercury/fees.hpp>
#include <mercury/version.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <optional>
#include <string>

namespace py = pybind11;
using namespace pybind11::literals;

namespace {

mercury::RiskLimits make_limits(std::uint64_t max_order_quantity,
                                std::uint64_t max_abs_position) {
  return mercury::RiskLimits{
      .max_order_quantity = mercury::Quantity{max_order_quantity},
      .max_abs_position = max_abs_position,
  };
}

mercury::FeeSchedule make_fees(std::int64_t maker_bps, std::int64_t taker_bps) {
  return mercury::FeeSchedule{.maker_bps = maker_bps, .taker_bps = taker_bps};
}

py::dict trade_to_dict(const mercury::Trade& trade) {
  return py::dict(
      "maker_id"_a = trade.maker_id.value(),
      "taker_id"_a = trade.taker_id.value(),
      "maker_account"_a = trade.maker_account.value(),
      "taker_account"_a = trade.taker_account.value(),
      "price"_a = trade.price.ticks(),
      "quantity"_a = trade.quantity.value(),
      "symbol"_a = trade.symbol.value(),
      "maker_fee"_a = trade.maker_fee,
      "taker_fee"_a = trade.taker_fee);
}

py::list trades_to_list(const std::vector<mercury::Trade>& trades) {
  py::list out;
  for (const auto& trade : trades) {
    out.append(trade_to_dict(trade));
  }
  return out;
}

py::dict submit_to_dict(const mercury::SubmitResult& result) {
  return py::dict("decision"_a = py::cast(result.decision),
                  "trades"_a = trades_to_list(result.trades));
}

py::dict level_to_dict(const mercury::BookLevel& level) {
  return py::dict("price"_a = level.price.ticks(),
                  "quantity"_a = level.quantity.value(),
                  "order_count"_a = level.order_count);
}

py::dict snapshot_to_dict(const mercury::BookSnapshot& snap) {
  py::list bids;
  py::list asks;
  for (const auto& level : snap.bids) {
    bids.append(level_to_dict(level));
  }
  for (const auto& level : snap.asks) {
    asks.append(level_to_dict(level));
  }
  py::object best_bid = py::none();
  py::object best_ask = py::none();
  py::object spread = py::none();
  if (const auto bid = snap.best_bid()) {
    best_bid = py::int_(bid->ticks());
  }
  if (const auto ask = snap.best_ask()) {
    best_ask = py::int_(ask->ticks());
  }
  if (const auto ticks = snap.spread_ticks()) {
    spread = py::int_(*ticks);
  }
  return py::dict("bids"_a = std::move(bids), "asks"_a = std::move(asks),
                  "best_bid"_a = best_bid, "best_ask"_a = best_ask,
                  "spread_ticks"_a = spread);
}

mercury::Engine make_engine(const mercury::RiskLimits& limits,
                            mercury::SelfTradePrevention stp,
                            const mercury::FeeSchedule& fees) {
  return mercury::Engine{limits, stp, fees};
}

}  // namespace

PYBIND11_MODULE(mercury_engine, m) {
  using mercury::AccountId;
  using mercury::Engine;
  using mercury::FeeSchedule;
  using mercury::MassCancelFilter;
  using mercury::Order;
  using mercury::OrderId;
  using mercury::Price;
  using mercury::Quantity;
  using mercury::RiskDecision;
  using mercury::RiskLimits;
  using mercury::SelfTradePrevention;
  using mercury::Side;
  using mercury::StopOrder;
  using mercury::Symbol;
  using mercury::TimeInForce;

  m.doc() = "Mercury Exchange Engine bindings";
  m.attr("__version__") = py::str(std::string(mercury::project_name()));

  py::enum_<Side>(m, "Side")
      .value("Buy", Side::Buy)
      .value("Sell", Side::Sell)
      .export_values();

  py::enum_<TimeInForce>(m, "TimeInForce")
      .value("Gtc", TimeInForce::Gtc)
      .value("Ioc", TimeInForce::Ioc)
      .value("Fok", TimeInForce::Fok)
      .export_values();

  py::enum_<RiskDecision>(m, "RiskDecision")
      .value("Accept", RiskDecision::Accept)
      .value("OrderTooLarge", RiskDecision::OrderTooLarge)
      .value("PositionLimit", RiskDecision::PositionLimit)
      .value("PostOnly", RiskDecision::PostOnly)
      .value("ReduceOnly", RiskDecision::ReduceOnly)
      .export_values();

  py::enum_<SelfTradePrevention>(m, "SelfTradePrevention")
      .value("Off", SelfTradePrevention::Off)
      .value("CancelResting", SelfTradePrevention::CancelResting)
      .export_values();

  py::class_<RiskLimits>(m, "RiskLimits")
      .def(py::init(&make_limits), py::arg("max_order_quantity") = 0,
           py::arg("max_abs_position") = 0)
      .def_property_readonly(
          "max_order_quantity",
          [](const RiskLimits& limits) { return limits.max_order_quantity.value(); })
      .def_property_readonly(
          "max_abs_position",
          [](const RiskLimits& limits) { return limits.max_abs_position; });

  py::class_<FeeSchedule>(m, "FeeSchedule")
      .def(py::init(&make_fees), py::arg("maker_bps") = 0, py::arg("taker_bps") = 0)
      .def_readonly("maker_bps", &FeeSchedule::maker_bps)
      .def_readonly("taker_bps", &FeeSchedule::taker_bps);

  py::class_<Engine>(m, "Engine")
      .def(py::init<>())
      .def(py::init(&make_engine), py::arg("limits") = RiskLimits{},
           py::arg("stp") = SelfTradePrevention::Off, py::arg("fees") = FeeSchedule{})
      .def(
          "add_limit",
          [](Engine& engine, std::uint64_t id, Side side, std::int64_t price,
             std::uint64_t quantity, std::uint64_t account, TimeInForce tif,
             std::uint64_t symbol, bool post_only, bool reduce_only) {
            return submit_to_dict(engine.add(Order{
                .id = OrderId{id},
                .side = side,
                .price = Price{price},
                .quantity = Quantity{quantity},
                .account = AccountId{account},
                .tif = tif,
                .symbol = Symbol{symbol},
                .post_only = post_only,
                .reduce_only = reduce_only,
            }));
          },
          py::arg("id"), py::arg("side"), py::arg("price"), py::arg("quantity"),
          py::arg("account") = 0, py::arg("tif") = TimeInForce::Gtc,
          py::arg("symbol") = 0, py::arg("post_only") = false,
          py::arg("reduce_only") = false)
      .def(
          "add_market",
          [](Engine& engine, std::uint64_t id, Side side, std::uint64_t quantity,
             std::uint64_t account, std::uint64_t symbol, bool reduce_only) {
            return submit_to_dict(engine.add_market(mercury::MarketOrder{
                .id = OrderId{id},
                .side = side,
                .quantity = Quantity{quantity},
                .account = AccountId{account},
                .symbol = Symbol{symbol},
                .reduce_only = reduce_only,
            }));
          },
          py::arg("id"), py::arg("side"), py::arg("quantity"),
          py::arg("account") = 0, py::arg("symbol") = 0,
          py::arg("reduce_only") = false)
      .def(
          "add_stop",
          [](Engine& engine, std::uint64_t id, Side side, std::int64_t stop_price,
             std::uint64_t quantity, std::uint64_t account,
             std::optional<std::int64_t> limit_price, TimeInForce tif,
             std::uint64_t symbol) {
            StopOrder stop{
                .id = OrderId{id},
                .side = side,
                .stop_price = Price{stop_price},
                .quantity = Quantity{quantity},
                .account = AccountId{account},
                .limit_price = std::nullopt,
                .tif = tif,
                .symbol = Symbol{symbol},
            };
            if (limit_price) {
              stop.limit_price = Price{*limit_price};
            }
            return submit_to_dict(engine.add_stop(std::move(stop)));
          },
          py::arg("id"), py::arg("side"), py::arg("stop_price"),
          py::arg("quantity"), py::arg("account") = 0,
          py::arg("limit_price") = py::none(), py::arg("tif") = TimeInForce::Gtc,
          py::arg("symbol") = 0)
      .def("cancel",
           [](Engine& engine, std::uint64_t id) {
             return engine.cancel(OrderId{id});
           },
           py::arg("id"))
      .def(
          "replace",
          [](Engine& engine, std::uint64_t id, std::int64_t price,
             std::uint64_t quantity) -> py::object {
            auto result = engine.replace(OrderId{id}, Price{price}, Quantity{quantity});
            if (!result) {
              return py::none();
            }
            return submit_to_dict(*result);
          },
          py::arg("id"), py::arg("price"), py::arg("quantity"))
      .def(
          "mass_cancel",
          [](Engine& engine, std::optional<std::uint64_t> account,
             std::optional<std::uint64_t> symbol, std::optional<Side> side) {
            MassCancelFilter filter;
            if (account) {
              filter.account = AccountId{*account};
            }
            if (symbol) {
              filter.symbol = Symbol{*symbol};
            }
            filter.side = side;
            return engine.mass_cancel(filter);
          },
          py::arg("account") = py::none(), py::arg("symbol") = py::none(),
          py::arg("side") = py::none())
      .def(
          "snapshot",
          [](const Engine& engine, std::size_t max_levels, std::uint64_t symbol) {
            return snapshot_to_dict(engine.snapshot(max_levels, Symbol{symbol}));
          },
          py::arg("max_levels") = 10, py::arg("symbol") = 0)
      .def(
          "position",
          [](const Engine& engine, std::uint64_t account, std::uint64_t symbol) {
            return engine.positions().quantity(AccountId{account}, Symbol{symbol});
          },
          py::arg("account"), py::arg("symbol") = 0)
      .def(
          "realized_pnl",
          [](const Engine& engine, std::uint64_t account, std::uint64_t symbol) {
            return engine.realized_pnl(AccountId{account}, Symbol{symbol});
          },
          py::arg("account"), py::arg("symbol") = 0)
      .def(
          "unrealized_pnl",
          [](const Engine& engine, std::uint64_t account,
             std::uint64_t symbol) -> py::object {
            const auto pnl = engine.unrealized_pnl(AccountId{account}, Symbol{symbol});
            if (!pnl) {
              return py::none();
            }
            return py::int_(*pnl);
          },
          py::arg("account"), py::arg("symbol") = 0)
      .def(
          "fees_paid",
          [](const Engine& engine, std::uint64_t account) {
            return engine.fees_paid(AccountId{account});
          },
          py::arg("account"))
      .def(
          "last_trade_price",
          [](const Engine& engine, std::uint64_t symbol) -> py::object {
            const auto price = engine.last_trade_price(Symbol{symbol});
            if (!price) {
              return py::none();
            }
            return py::int_(price->ticks());
          },
          py::arg("symbol") = 0)
      .def(
          "pending_stop_count",
          [](const Engine& engine, std::uint64_t symbol) {
            return engine.pending_stop_count(Symbol{symbol});
          },
          py::arg("symbol") = 0);
}
