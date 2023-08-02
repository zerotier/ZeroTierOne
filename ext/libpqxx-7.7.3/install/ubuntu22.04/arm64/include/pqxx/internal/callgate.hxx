#ifndef PQXX_H_CALLGATE
#define PQXX_H_CALLGATE

/*
Here's what a typical gate class definition looks like:

#include <pqxx/internal/callgate.hxx>

namespace pqxx::internal::gate
{
class PQXX_PRIVATE @gateclass@ : callgate<@host@>
{
  friend class @client@;

  @gateclass@(reference x) : super(x) {}

  // Methods here.  Use home() to access the host-class object.
};
} // namespace pqxx::internal::gate
*/

namespace pqxx::internal
{
/// Base class for call gates.
/**
 * A call gate defines a limited, private interface on the host class that
 * specified client classes can access.
 *
 * The metaphor works as follows: the gate stands in front of a "home," which
 * is really a class, and only lets specific friends in.
 *
 * To implement a call gate that gives client C access to host H,
 * * derive a gate class from callgate<H>;
 * * make the gate class a friend of H;
 * * make C a friend of the gate class; and
 * * implement "stuff C can do with H" as private members in the gate class.
 *
 * This special kind of "gated" friendship gives C private access to H, but
 * only through an expressly limited interface.  The gate class can access its
 * host object as home().
 *
 * Keep gate classes entirely stateless.  They should be ultra-lightweight
 * wrappers for their host classes, and be optimized away as much as possible
 * by the compiler.  Once you start adding state, you're on a slippery slope
 * away from the pure, clean, limited interface pattern that gate classes are
 * meant to implement.
 *
 * Ideally, all member functions of the gate class should be one-liners passing
 * calls straight on to the host class.  It can be useful however to break this
 * rule temporarily during inter-class refactoring.
 */
template<typename HOME> class PQXX_PRIVATE callgate
{
protected:
  /// This class, to keep constructors easy.
  using super = callgate<HOME>;
  /// A reference to the host class.  Helps keep constructors easy.
  using reference = HOME &;

  callgate(reference x) : m_home(x) {}

  /// The home object.  The gate class has full "private" access.
  reference home() const noexcept { return m_home; }

private:
  reference m_home;
};
} // namespace pqxx::internal

#endif
