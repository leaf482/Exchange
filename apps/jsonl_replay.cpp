#include "mercury/jsonl.hpp"

#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: jsonl_replay <events.jsonl>\n";
    return 1;
  }

  std::ifstream input(argv[1]);
  if (!input) {
    std::cerr << "failed to open " << argv[1] << '\n';
    return 1;
  }

  const auto log = mercury::jsonl::load_event_log(input);
  mercury::OrderBook book;
  const auto trades = mercury::replay(book, log);

  for (const auto& trade : trades) {
    std::cout << "{\"maker_id\":" << trade.maker_id.value()
              << ",\"taker_id\":" << trade.taker_id.value()
              << ",\"maker_account\":" << trade.maker_account.value()
              << ",\"taker_account\":" << trade.taker_account.value()
              << ",\"price\":" << trade.price.ticks()
              << ",\"quantity\":" << trade.quantity.value() << "}\n";
  }

  return 0;
}
