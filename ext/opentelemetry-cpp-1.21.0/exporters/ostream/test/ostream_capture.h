// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <iostream>
#include <sstream>
#include <string>

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace ostream
{
namespace test
{
/**
 * The OStreamCapture captures from the specified stream for its lifetime
 */
class OStreamCapture
{
public:
  /**
   * Create a OStreamCapture which will capture the output of the ostream that it was constructed
   * with for the lifetime of the instance.
   */
  OStreamCapture(std::ostream &ostream) : stream_(ostream), buf_(ostream.rdbuf())
  {
    stream_.rdbuf(captured_.rdbuf());
  }

  ~OStreamCapture() { stream_.rdbuf(buf_); }

  /**
   * Returns the captured data from the stream.
   */
  std::string GetCaptured() const { return captured_.str(); }

private:
  std::ostream &stream_;
  std::streambuf *buf_;
  std::stringstream captured_;
};

/**
 * Helper method to invoke the passed func while recording the output of the specified stream and
 * return the output afterwards.
 */
template <typename Func>
std::string WithOStreamCapture(std::ostream &stream, Func func)
{
  OStreamCapture capture(stream);
  func();
  return capture.GetCaptured();
}

}  // namespace test
}  // namespace ostream
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
