// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdlib.h>
#include <limits>

#include "opentelemetry/sdk/metrics/aggregation/base2_exponential_histogram_indexer.h"

using namespace opentelemetry::sdk::metrics;

TEST(Base2ExponentialHistogramIndexerTest, ScaleOne)
{
  const Base2ExponentialHistogramIndexer indexer{1};
  auto compute_index = [indexer](double value) { return indexer.ComputeIndex(value); };

  EXPECT_EQ(compute_index(std::numeric_limits<double>::max()), 2047);
  EXPECT_EQ(compute_index(strtod("0x1p1023", nullptr)), 2045);
  EXPECT_EQ(compute_index(strtod("0x1.1p1023", nullptr)), 2046);
  EXPECT_EQ(compute_index(strtod("0x1p1022", nullptr)), 2043);
  EXPECT_EQ(compute_index(strtod("0x1.1p1022", nullptr)), 2044);
  EXPECT_EQ(compute_index(strtod("0x1p-1022", nullptr)), -2045);
  EXPECT_EQ(compute_index(strtod("0x1.1p-1022", nullptr)), -2044);
  EXPECT_EQ(compute_index(strtod("0x1p-1021", nullptr)), -2043);
  EXPECT_EQ(compute_index(strtod("0x1.1p-1021", nullptr)), -2042);
  EXPECT_EQ(compute_index(std::numeric_limits<double>::min()), -2045);
  EXPECT_EQ(compute_index(std::numeric_limits<double>::denorm_min()), -2149);
  EXPECT_EQ(compute_index(15.0), 7);
  EXPECT_EQ(compute_index(9.0), 6);
  EXPECT_EQ(compute_index(7.0), 5);
  EXPECT_EQ(compute_index(5.0), 4);
  EXPECT_EQ(compute_index(3.0), 3);
  EXPECT_EQ(compute_index(2.5), 2);
  EXPECT_EQ(compute_index(1.5), 1);
  EXPECT_EQ(compute_index(1.2), 0);
  EXPECT_EQ(compute_index(1.0), -1);
  EXPECT_EQ(compute_index(0.75), -1);
  EXPECT_EQ(compute_index(0.55), -2);
  EXPECT_EQ(compute_index(0.45), -3);
}

TEST(Base2ExponentialHistogramIndexerTest, ScaleZero)
{
  const Base2ExponentialHistogramIndexer indexer{0};
  auto compute_index = [indexer](double value) { return indexer.ComputeIndex(value); };

  // Near +Inf.
  // Use constant for exp, as max_exponent constant includes bias.
  EXPECT_EQ(compute_index(std::numeric_limits<double>::max()), 1023);
  EXPECT_EQ(compute_index(strtod("0x1p1023", nullptr)), 1022);
  EXPECT_EQ(compute_index(strtod("0x1.1p1023", nullptr)), 1023);
  EXPECT_EQ(compute_index(strtod("0x1p1022", nullptr)), 1021);
  EXPECT_EQ(compute_index(strtod("0x1.1p1022", nullptr)), 1022);
  // Near 0.
  EXPECT_EQ(compute_index(strtod("0x1p-1022", nullptr)), -1023);
  EXPECT_EQ(compute_index(strtod("0x1.1p-1022", nullptr)), -1022);
  EXPECT_EQ(compute_index(strtod("0x1p-1021", nullptr)), -1022);
  EXPECT_EQ(compute_index(strtod("0x1.1p-1021", nullptr)), -1021);
  EXPECT_EQ(compute_index(std::numeric_limits<double>::min()), -1023);
  EXPECT_EQ(compute_index(std::numeric_limits<double>::denorm_min()), -1075);
  // Near 1.
  EXPECT_EQ(compute_index(4.0), 1);
  EXPECT_EQ(compute_index(3.0), 1);
  EXPECT_EQ(compute_index(2.0), 0);
  EXPECT_EQ(compute_index(1.5), 0);
  EXPECT_EQ(compute_index(1.0), -1);
  EXPECT_EQ(compute_index(0.75), -1);
  EXPECT_EQ(compute_index(0.51), -1);
  EXPECT_EQ(compute_index(0.5), -2);
  EXPECT_EQ(compute_index(0.26), -2);
  EXPECT_EQ(compute_index(0.25), -3);
  EXPECT_EQ(compute_index(0.126), -3);
  EXPECT_EQ(compute_index(0.125), -4);
}

TEST(Base2ExponentialHistogramIndexerTest, ScaleNegativeOne)
{
  const Base2ExponentialHistogramIndexer indexer{-1};
  auto compute_index = [indexer](double value) { return indexer.ComputeIndex(value); };

  EXPECT_EQ(compute_index(17.0), 2);
  EXPECT_EQ(compute_index(16.0), 1);
  EXPECT_EQ(compute_index(15.0), 1);
  EXPECT_EQ(compute_index(9.0), 1);
  EXPECT_EQ(compute_index(8.0), 1);
  EXPECT_EQ(compute_index(5.0), 1);
  EXPECT_EQ(compute_index(4.0), 0);
  EXPECT_EQ(compute_index(3.0), 0);
  EXPECT_EQ(compute_index(2.0), 0);
  EXPECT_EQ(compute_index(1.5), 0);
  EXPECT_EQ(compute_index(1.0), -1);
  EXPECT_EQ(compute_index(0.75), -1);
  EXPECT_EQ(compute_index(0.5), -1);
  EXPECT_EQ(compute_index(0.25), -2);
  EXPECT_EQ(compute_index(0.20), -2);
  EXPECT_EQ(compute_index(0.13), -2);
  EXPECT_EQ(compute_index(0.125), -2);
  EXPECT_EQ(compute_index(0.10), -2);
  EXPECT_EQ(compute_index(0.0625), -3);
  EXPECT_EQ(compute_index(0.06), -3);
}

TEST(Base2ExponentialHistogramIndexerTest, ScaleNegativeFour)
{
  const Base2ExponentialHistogramIndexer indexer{-4};
  auto compute_index = [indexer](double value) { return indexer.ComputeIndex(value); };

  EXPECT_EQ(compute_index(strtod("0x1p0", nullptr)), -1);
  EXPECT_EQ(compute_index(strtod("0x10p0", nullptr)), 0);
  EXPECT_EQ(compute_index(strtod("0x100p0", nullptr)), 0);
  EXPECT_EQ(compute_index(strtod("0x1000p0", nullptr)), 0);
  EXPECT_EQ(compute_index(strtod("0x10000p0", nullptr)), 0);  // Base == 2**16
  EXPECT_EQ(compute_index(strtod("0x100000p0", nullptr)), 1);
  EXPECT_EQ(compute_index(strtod("0x1000000p0", nullptr)), 1);
  EXPECT_EQ(compute_index(strtod("0x10000000p0", nullptr)), 1);
  EXPECT_EQ(compute_index(strtod("0x100000000p0", nullptr)), 1);  // == 2**32
  EXPECT_EQ(compute_index(strtod("0x1000000000p0", nullptr)), 2);
  EXPECT_EQ(compute_index(strtod("0x10000000000p0", nullptr)), 2);
  EXPECT_EQ(compute_index(strtod("0x100000000000p0", nullptr)), 2);
  EXPECT_EQ(compute_index(strtod("0x1000000000000p0", nullptr)), 2);  // 2**48
  EXPECT_EQ(compute_index(strtod("0x10000000000000p0", nullptr)), 3);
  EXPECT_EQ(compute_index(strtod("0x100000000000000p0", nullptr)), 3);
  EXPECT_EQ(compute_index(strtod("0x1000000000000000p0", nullptr)), 3);
  EXPECT_EQ(compute_index(strtod("0x10000000000000000p0", nullptr)), 3);  // 2**64
  EXPECT_EQ(compute_index(strtod("0x100000000000000000p0", nullptr)), 4);
  EXPECT_EQ(compute_index(strtod("0x1000000000000000000p0", nullptr)), 4);
  EXPECT_EQ(compute_index(strtod("0x10000000000000000000p0", nullptr)), 4);
  EXPECT_EQ(compute_index(strtod("0x100000000000000000000p0", nullptr)), 4);
  EXPECT_EQ(compute_index(strtod("0x1000000000000000000000p0", nullptr)), 5);
  EXPECT_EQ(compute_index(1 / strtod("0x1p0", nullptr)), -1);
  EXPECT_EQ(compute_index(1 / strtod("0x10p0", nullptr)), -1);
  EXPECT_EQ(compute_index(1 / strtod("0x100p0", nullptr)), -1);
  EXPECT_EQ(compute_index(1 / strtod("0x1000p0", nullptr)), -1);
  EXPECT_EQ(compute_index(1 / strtod("0x10000p0", nullptr)), -2);  // 2**-16
  EXPECT_EQ(compute_index(1 / strtod("0x100000p0", nullptr)), -2);
  EXPECT_EQ(compute_index(1 / strtod("0x1000000p0", nullptr)), -2);
  EXPECT_EQ(compute_index(1 / strtod("0x10000000p0", nullptr)), -2);
  EXPECT_EQ(compute_index(1 / strtod("0x100000000p0", nullptr)), -3);  // 2**-32
  EXPECT_EQ(compute_index(1 / strtod("0x1000000000p0", nullptr)), -3);
  EXPECT_EQ(compute_index(1 / strtod("0x10000000000p0", nullptr)), -3);
  EXPECT_EQ(compute_index(1 / strtod("0x100000000000p0", nullptr)), -3);
  EXPECT_EQ(compute_index(1 / strtod("0x1000000000000p0", nullptr)), -4);  // 2**-48
  EXPECT_EQ(compute_index(1 / strtod("0x10000000000000p0", nullptr)), -4);
  EXPECT_EQ(compute_index(1 / strtod("0x100000000000000p0", nullptr)), -4);
  EXPECT_EQ(compute_index(1 / strtod("0x1000000000000000p0", nullptr)), -4);
  EXPECT_EQ(compute_index(1 / strtod("0x10000000000000000p0", nullptr)), -5);  // 2**-64
  EXPECT_EQ(compute_index(1 / strtod("0x100000000000000000p0", nullptr)), -5);
  // Max values.
  EXPECT_EQ(compute_index(0x1.FFFFFFFFFFFFFp1023), 63);
  EXPECT_EQ(compute_index(strtod("0x1p1023", nullptr)), 63);
  EXPECT_EQ(compute_index(strtod("0x1p1019", nullptr)), 63);
  EXPECT_EQ(compute_index(strtod("0x1p1009", nullptr)), 63);
  EXPECT_EQ(compute_index(strtod("0x1p1008", nullptr)), 62);
  EXPECT_EQ(compute_index(strtod("0x1p1007", nullptr)), 62);
  EXPECT_EQ(compute_index(strtod("0x1p1000", nullptr)), 62);
  EXPECT_EQ(compute_index(strtod("0x1p0993", nullptr)), 62);
  EXPECT_EQ(compute_index(strtod("0x1p0992", nullptr)), 61);
  EXPECT_EQ(compute_index(strtod("0x1p0991", nullptr)), 61);
  // Min and subnormal values.
  EXPECT_EQ(compute_index(strtod("0x1p-1074", nullptr)), -68);
  EXPECT_EQ(compute_index(strtod("0x1p-1073", nullptr)), -68);
  EXPECT_EQ(compute_index(strtod("0x1p-1072", nullptr)), -68);
  EXPECT_EQ(compute_index(strtod("0x1p-1057", nullptr)), -67);
  EXPECT_EQ(compute_index(strtod("0x1p-1056", nullptr)), -67);
  EXPECT_EQ(compute_index(strtod("0x1p-1041", nullptr)), -66);
  EXPECT_EQ(compute_index(strtod("0x1p-1040", nullptr)), -66);
  EXPECT_EQ(compute_index(strtod("0x1p-1025", nullptr)), -65);
  EXPECT_EQ(compute_index(strtod("0x1p-1024", nullptr)), -65);
  EXPECT_EQ(compute_index(strtod("0x1p-1023", nullptr)), -64);
  EXPECT_EQ(compute_index(strtod("0x1p-1022", nullptr)), -64);
  EXPECT_EQ(compute_index(strtod("0x1p-1009", nullptr)), -64);
  EXPECT_EQ(compute_index(strtod("0x1p-1008", nullptr)), -64);
  EXPECT_EQ(compute_index(strtod("0x1p-1007", nullptr)), -63);
  EXPECT_EQ(compute_index(strtod("0x1p-0993", nullptr)), -63);
  EXPECT_EQ(compute_index(strtod("0x1p-0992", nullptr)), -63);
  EXPECT_EQ(compute_index(strtod("0x1p-0991", nullptr)), -62);
  EXPECT_EQ(compute_index(strtod("0x1p-0977", nullptr)), -62);
  EXPECT_EQ(compute_index(strtod("0x1p-0976", nullptr)), -62);
  EXPECT_EQ(compute_index(strtod("0x1p-0975", nullptr)), -61);
}
