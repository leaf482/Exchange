#include "mercury/engine.hpp"

#include <gtest/gtest.h>

using mercury::AccountId;
using mercury::Engine;
using mercury::Order;
using mercury::OrderId;
using mercury::Price;
using mercury::Quantity;
using mercury::RiskDecision;
using mercury::RiskLimits;
using mercury::SelfTradePrevention;
using mercury::Side;

TEST(EngineStp, FreesWorkingExposureOnStpCancel) {
  Engine engine{RiskLimits{.max_abs_position = 5},
                SelfTradePrevention::CancelResting};

  ASSERT_EQ(engine
                .add(Order{.id = OrderId{1},
                           .side = Side::Sell,
                           .price = Price{100},
                           .quantity = Quantity{5},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);

  // Self-trade cancels resting sell; working short exposure must clear.
  ASSERT_EQ(engine
                .add(Order{.id = OrderId{2},
                           .side = Side::Buy,
                           .price = Price{100},
                           .quantity = Quantity{1},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);

  EXPECT_FALSE(engine.book().best_ask());
  // Same account can rest a new sell of 5 again (exposure freed).
  EXPECT_EQ(engine
                .add(Order{.id = OrderId{3},
                           .side = Side::Sell,
                           .price = Price{101},
                           .quantity = Quantity{5},
                           .account = AccountId{1}})
                .decision,
            RiskDecision::Accept);
}
