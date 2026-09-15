#include "mercury/jsonl.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void write_report(std::ostream& out, const mercury::AccountReport& report) {
  out << "{\"account\":" << report.account.value()
      << ",\"cash\":" << report.cash
      << ",\"reserved\":" << report.reserved
      << ",\"available\":" << report.available
      << ",\"fees_paid\":" << report.fees_paid
      << ",\"realized_pnl\":" << report.realized_pnl
      << ",\"inventory_mark\":" << report.inventory_mark
      << ",\"equity\":" << report.equity;
  if (report.unrealized_pnl) {
    out << ",\"unrealized_pnl\":" << *report.unrealized_pnl;
  } else {
    out << ",\"unrealized_pnl\":null";
  }
  out << ",\"positions\":[";
  for (std::size_t i = 0; i < report.positions.size(); ++i) {
    const auto& row = report.positions[i];
    if (i != 0) {
      out << ',';
    }
    out << "{\"symbol\":" << row.symbol.value()
        << ",\"quantity\":" << row.quantity
        << ",\"avg_ticks\":" << row.avg_ticks
        << ",\"realized_pnl\":" << row.realized_pnl;
    if (row.mark_ticks) {
      out << ",\"mark_ticks\":" << *row.mark_ticks;
    } else {
      out << ",\"mark_ticks\":null";
    }
    if (row.unrealized_pnl) {
      out << ",\"unrealized_pnl\":" << *row.unrealized_pnl;
    } else {
      out << ",\"unrealized_pnl\":null";
    }
    out << '}';
  }
  out << "]}\n";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3 || argc > 4) {
    std::cerr << "usage: account_report <events.jsonl> <account> [last|mid]\n";
    return 1;
  }

  mercury::MarkSource mark = mercury::MarkSource::LastTrade;
  if (argc == 4) {
    const std::string mode = argv[3];
    if (mode == "mid") {
      mark = mercury::MarkSource::Mid;
    } else if (mode != "last") {
      std::cerr << "mark must be last or mid\n";
      return 1;
    }
  }

  std::ifstream input(argv[1]);
  if (!input) {
    std::cerr << "failed to open " << argv[1] << '\n';
    return 1;
  }

  const auto log = mercury::jsonl::load_event_log(input);
  mercury::Engine engine;
  mercury::replay(engine, log);
  write_report(std::cout,
               engine.account_report(
                   mercury::AccountId{static_cast<std::uint64_t>(std::stoull(argv[2]))},
                   mark));
  return 0;
}
