#include "mercury/engine.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <vector>

using mercury::AccountId;
using mercury::Engine;
using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::Side;
using mercury::Symbol;

namespace {

double percentile(std::vector<double> values, double p) {
  std::sort(values.begin(), values.end());
  const auto index = static_cast<std::size_t>(p * (values.size() - 1));
  return values[index];
}

void configure_latency(benchmark::internal::Benchmark* bench) {
  bench->Unit(benchmark::kNanosecond)
      ->Repetitions(20)
      ->ReportAggregatesOnly(true)
      ->ComputeStatistics("p95",
                         [](const std::vector<double>& v) { return percentile(v, 0.95); })
      ->ComputeStatistics("p99",
                         [](const std::vector<double>& v) { return percentile(v, 0.99); });
}

Order make_order(std::uint64_t id, Side side, Price price, std::uint64_t qty,
                 AccountId account, Symbol symbol = Symbol{0}) {
  return Order{
      .id = OrderId{id},
      .side = side,
      .price = price,
      .quantity = Quantity{qty},
      .account = account,
      .symbol = symbol,
  };
}

}  // namespace

// Non-crossing limit rests on the book.
static void BM_RestLimit(benchmark::State& state) {
  std::uint64_t id = 1;
  for (auto _ : state) {
    state.PauseTiming();
    Engine engine;
    auto order = make_order(id++, Side::Buy, Price{100}, 1, AccountId{1});
    state.ResumeTiming();

    auto result = engine.add(std::move(order));
    benchmark::DoNotOptimize(result);
  }
}

// One resting sell, then a crossing buy that fully fills.
static void BM_MatchLimit(benchmark::State& state) {
  std::uint64_t id = 1;
  for (auto _ : state) {
    state.PauseTiming();
    Engine engine;
    engine.add(make_order(id++, Side::Sell, Price{100}, 1, AccountId{1}));
    auto buy = make_order(id++, Side::Buy, Price{100}, 1, AccountId{2});
    state.ResumeTiming();

    auto result = engine.add(std::move(buy));
    benchmark::DoNotOptimize(result);
  }
}

// Rest one order, then cancel it.
static void BM_Cancel(benchmark::State& state) {
  std::uint64_t id = 1;
  for (auto _ : state) {
    state.PauseTiming();
    Engine engine;
    const OrderId order_id{id};
    engine.add(make_order(id++, Side::Buy, Price{100}, 1, AccountId{1}));
    state.ResumeTiming();

    bool cancelled = engine.cancel(order_id);
    benchmark::DoNotOptimize(cancelled);
  }
}

// Sweep N ask price levels with one aggressive buy.
static void BM_MatchDeepBook(benchmark::State& state) {
  const int levels = static_cast<int>(state.range(0));
  std::uint64_t id = 1;
  for (auto _ : state) {
    state.PauseTiming();
    Engine engine;
    for (int level = 0; level < levels; ++level) {
      engine.add(make_order(id++, Side::Sell, Price{100 + level}, 1, AccountId{1}));
    }
    auto buy = make_order(id++, Side::Buy, Price{100 + levels - 1},
                          static_cast<std::uint64_t>(levels), AccountId{2});
    state.ResumeTiming();

    auto result = engine.add(std::move(buy));
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_RestLimit)->Apply(configure_latency);
BENCHMARK(BM_MatchLimit)->Apply(configure_latency);
BENCHMARK(BM_Cancel)->Apply(configure_latency);
BENCHMARK(BM_MatchDeepBook)->Apply(configure_latency)->Arg(8)->Arg(32)->Arg(128);

// Rest one order on each of N symbols, then match on a single symbol.
static void BM_MatchOneOfManySymbols(benchmark::State& state) {
  const int symbols = static_cast<int>(state.range(0));
  std::uint64_t id = 1;
  for (auto _ : state) {
    state.PauseTiming();
    Engine engine;
    for (int symbol = 0; symbol < symbols; ++symbol) {
      engine.add(make_order(id++, Side::Sell, Price{100}, 1, AccountId{1},
                            Symbol{static_cast<std::uint64_t>(symbol)}));
    }
    auto buy = make_order(id++, Side::Buy, Price{100}, 1, AccountId{2}, Symbol{0});
    state.ResumeTiming();

    auto result = engine.add(std::move(buy));
    benchmark::DoNotOptimize(result);
  }
}

// Cancel all resting orders for one account across N symbols.
static void BM_MassCancelAcrossSymbols(benchmark::State& state) {
  const int symbols = static_cast<int>(state.range(0));
  std::uint64_t id = 1;
  for (auto _ : state) {
    state.PauseTiming();
    Engine engine;
    for (int symbol = 0; symbol < symbols; ++symbol) {
      engine.add(make_order(id++, Side::Buy, Price{100}, 1, AccountId{1},
                            Symbol{static_cast<std::uint64_t>(symbol)}));
      engine.add(make_order(id++, Side::Buy, Price{99}, 1, AccountId{2},
                            Symbol{static_cast<std::uint64_t>(symbol)}));
    }
    state.ResumeTiming();

    auto cancelled =
        engine.mass_cancel(mercury::MassCancelFilter{.account = AccountId{1},
                                                    .symbol = std::nullopt,
                                                    .side = std::nullopt});
    benchmark::DoNotOptimize(cancelled);
  }
}

BENCHMARK(BM_MatchOneOfManySymbols)
    ->Apply(configure_latency)
    ->Arg(1)
    ->Arg(8)
    ->Arg(32);
BENCHMARK(BM_MassCancelAcrossSymbols)
    ->Apply(configure_latency)
    ->Arg(8)
    ->Arg(32);

BENCHMARK_MAIN();
