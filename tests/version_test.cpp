#include "mercury/version.hpp"

#include <gtest/gtest.h>

TEST(Version, ProjectName) {
  EXPECT_EQ(mercury::project_name(), "Mercury Exchange");
}
