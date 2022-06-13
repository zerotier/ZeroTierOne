#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

enum E {
  V,
};

struct S {
  uint8_t field;
};

using A = uint8_t;

static const S C1 = S{ /* .field = */ 0 };

static const E C2 = V;

static const A C3 = 0;
