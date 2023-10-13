#include "drake/math/unit_vector.h"

#include <gtest/gtest.h>

#include "drake/common/test_utilities/expect_no_throw.h"
#include "drake/common/test_utilities/expect_throws_message.h"

namespace drake {
namespace math {
namespace internal {
namespace {

GTEST_TEST(UnitVectorTest, ThrowOrWarnIfNotUnitVector) {
  double vector_mag_squared;

  // Verify that no exception is thrown for a valid unit vector.
  Vector3<double> unit_vector(1.0, 0.0, 0.0);
  DRAKE_EXPECT_NO_THROW(vector_mag_squared =
      ThrowIfNotUnitVector(unit_vector, "UnusedFunctionName"));
  EXPECT_EQ(vector_mag_squared, unit_vector.squaredNorm());

  // No message should be written to the log file for a valid unit vector.
  vector_mag_squared = WarnIfNotUnitVector(unit_vector, "UnusedFunctionName");
  EXPECT_EQ(vector_mag_squared, unit_vector.squaredNorm());

  // Verify that no exception is thrown for a valid or near valid unit vector.
  unit_vector = Vector3<double>(4.321, M_PI, 97531.2468).normalized();
  DRAKE_EXPECT_NO_THROW(vector_mag_squared =
      ThrowIfNotUnitVector(unit_vector, "UnusedFunctionName"));
  EXPECT_EQ(vector_mag_squared, unit_vector.squaredNorm());

  // Verify that no exception is thrown when |unit_vector| is nearly 1.0.
  constexpr double kepsilon = std::numeric_limits<double>::epsilon();
  unit_vector = Vector3<double>(1 + kepsilon, 0, 0);
  DRAKE_EXPECT_NO_THROW(vector_mag_squared =
      ThrowIfNotUnitVector(unit_vector, "UnusedFunctionName"));
  EXPECT_EQ(vector_mag_squared, unit_vector.squaredNorm());
  EXPECT_NE(vector_mag_squared, 1.0);

  // Verify an exception is thrown for an invalid unit vector.
  Vector3<double> not_unit_vector(1.0, 2.0, 3.0);
  std::string expected_message =
      "SomeFunctionName\\(\\): The unit_vector argument 1 2 3 is"
      " not a unit vector.\n"
      "\\|unit_vector\\| = 3.74165738677\\d+\n"
      "\\|\\|unit_vector\\| - 1\\| = 2.74165738677\\d+ is greater than .*.";
  DRAKE_EXPECT_THROWS_MESSAGE(
      ThrowIfNotUnitVector(not_unit_vector, "SomeFunctionName"),
      expected_message);

  // A message should be written to the log file for an invalid unit vector.
  vector_mag_squared = WarnIfNotUnitVector(not_unit_vector, "SomeFunctionName");
  EXPECT_EQ(vector_mag_squared, not_unit_vector.squaredNorm());

  // Verify an exception is thrown for a unit vector with NAN elements.
  not_unit_vector = Vector3<double>(NAN, NAN, NAN);
  expected_message =
      "SomeFunctionName\\(\\): The unit_vector argument nan nan nan is"
      " not a unit vector.\n"
      "\\|unit_vector\\| = nan\n"
      "\\|\\|unit_vector\\| - 1\\| = nan is greater than .*.";
  DRAKE_EXPECT_THROWS_MESSAGE(
      ThrowIfNotUnitVector(not_unit_vector, "SomeFunctionName"),
      expected_message);

  // Verify an exception is thrown for a unit vector with infinity elements.
  constexpr double kInfinity = std::numeric_limits<double>::infinity();
  not_unit_vector = Vector3<double>(kInfinity, kInfinity, kInfinity);
  expected_message =
      "SomeFunctionName\\(\\): The unit_vector argument inf inf inf is"
      " not a unit vector.\n"
      "\\|unit_vector\\| = inf\n"
      "\\|\\|unit_vector\\| - 1\\| = inf is greater than .*.";
  DRAKE_EXPECT_THROWS_MESSAGE(
      ThrowIfNotUnitVector(not_unit_vector, "SomeFunctionName"),
      expected_message);
}

}  // namespace
}  // namespace internal
}  // namespace math
}  // namespace drake
