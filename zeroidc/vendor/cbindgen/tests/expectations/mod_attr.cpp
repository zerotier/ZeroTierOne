#if 0
DEF FOO = 0
DEF BAR = 0
#endif


#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>

#if defined(FOO)
static const int32_t FOO = 1;
#endif

#if defined(BAR)
static const int32_t BAR = 2;
#endif

#if defined(FOO)
struct Foo {

};
#endif

#if defined(BAR)
struct Bar {

};
#endif

extern "C" {

#if defined(FOO)
void foo(const Foo *foo);
#endif

#if defined(BAR)
void bar(const Bar *bar);
#endif

} // extern "C"
