#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

static const int32_t FOO = 10;

static const uint32_t DELIMITER = ':';

static const uint32_t LEFTCURLY = '{';

static const uint32_t QUOTE = '\'';

static const uint32_t TAB = '\t';

static const uint32_t NEWLINE = '\n';

static const uint32_t HEART = U'\U00002764';

static const uint32_t EQUID = U'\U00010083';

static const float ZOM = 3.14;

/// A single-line doc comment.
static const int8_t POS_ONE = 1;

/// A
/// multi-line
/// doc
/// comment.
static const int8_t NEG_ONE = -1;

static const int64_t SHIFT = 3;

static const int64_t XBOOL = 1;

static const int64_t XFALSE = ((0 << SHIFT) | XBOOL);

static const int64_t XTRUE = (1 << (SHIFT | XBOOL));

static const uint8_t CAST = (uint8_t)'A';

static const uint32_t DOUBLE_CAST = (uint32_t)(float)1;

struct Foo {
  int32_t x[FOO];
};

extern "C" {

void root(Foo x);

} // extern "C"
