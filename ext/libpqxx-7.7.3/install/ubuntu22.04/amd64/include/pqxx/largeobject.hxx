/* Large Objects interface.  Deprecated; use blob instead.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/largeobject instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_LARGEOBJECT
#define PQXX_H_LARGEOBJECT

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <streambuf>

#include "pqxx/dbtransaction.hxx"


namespace pqxx
{
/// Identity of a large object.
/** @deprecated Use the @ref blob class instead.
 *
 * Encapsulates the identity of a large object.
 *
 * A largeobject must be accessed only from within a backend transaction, but
 * the object's identity remains valid as long as the object exists.
 */
class PQXX_LIBEXPORT largeobject
{
public:
  using size_type = large_object_size_type;

  /// Refer to a nonexistent large object (similar to what a null pointer
  /// does).
  [[deprecated("Use blob instead.")]] largeobject() noexcept = default;

  /// Create new large object.
  /** @param t Backend transaction in which the object is to be created.
   */
  [[deprecated("Use blob instead.")]] explicit largeobject(dbtransaction &t);

  /// Wrap object with given oid.
  /** Convert combination of a transaction and object identifier into a
   * large object identity.  Does not affect the database.
   * @param o Object identifier for the given object.
   */
  [[deprecated("Use blob instead.")]] explicit largeobject(oid o) noexcept :
          m_id{o}
  {}

  /// Import large object from a local file.
  /** Creates a large object containing the data found in the given file.
   * @param t Backend transaction in which the large object is to be created.
   * @param file A filename on the client program's filesystem.
   */
  [[deprecated("Use blob instead.")]] largeobject(
    dbtransaction &t, std::string_view file);

  /// Take identity of an opened large object.
  /** Copy identity of already opened large object.  Note that this may be done
   * as an implicit conversion.
   * @param o Already opened large object to copy identity from.
   */
  [[deprecated("Use blob instead.")]] largeobject(
    largeobjectaccess const &o) noexcept;

  /// Object identifier.
  /** The number returned by this function identifies the large object in the
   * database we're connected to (or oid_none is returned if we refer to the
   * null object).
   */
  [[nodiscard]] oid id() const noexcept { return m_id; }

  /**
   * @name Identity comparisons
   *
   * These operators compare the object identifiers of large objects.  This has
   * nothing to do with the objects' actual contents; use them only for keeping
   * track of containers of references to large objects and such.
   */
  //@{
  /// Compare object identities
  /** @warning Only valid between large objects in the same database. */
  [[nodiscard]] bool operator==(largeobject const &other) const
  {
    return m_id == other.m_id;
  }
  /// Compare object identities
  /** @warning Only valid between large objects in the same database. */
  [[nodiscard]] bool operator!=(largeobject const &other) const
  {
    return m_id != other.m_id;
  }
  /// Compare object identities
  /** @warning Only valid between large objects in the same database. */
  [[nodiscard]] bool operator<=(largeobject const &other) const
  {
    return m_id <= other.m_id;
  }
  /// Compare object identities
  /** @warning Only valid between large objects in the same database. */
  [[nodiscard]] bool operator>=(largeobject const &other) const
  {
    return m_id >= other.m_id;
  }
  /// Compare object identities
  /** @warning Only valid between large objects in the same database. */
  [[nodiscard]] bool operator<(largeobject const &other) const
  {
    return m_id < other.m_id;
  }
  /// Compare object identities
  /** @warning Only valid between large objects in the same database. */
  [[nodiscard]] bool operator>(largeobject const &other) const
  {
    return m_id > other.m_id;
  }
  //@}

  /// Export large object's contents to a local file
  /** Writes the data stored in the large object to the given file.
   * @param t Transaction in which the object is to be accessed
   * @param file A filename on the client's filesystem
   */
  void to_file(dbtransaction &t, std::string_view file) const;

  /// Delete large object from database
  /** Unlike its low-level equivalent cunlink, this will throw an exception if
   * deletion fails.
   * @param t Transaction in which the object is to be deleted
   */
  void remove(dbtransaction &t) const;

protected:
  PQXX_PURE static internal::pq::PGconn *
  raw_connection(dbtransaction const &T);

  PQXX_PRIVATE std::string reason(connection const &, int err) const;

private:
  oid m_id = oid_none;
};


/// Accessor for large object's contents.
/** @deprecated Use the `blob` class instead.
 */
class PQXX_LIBEXPORT largeobjectaccess : private largeobject
{
public:
  using largeobject::size_type;
  using off_type = size_type;
  using pos_type = size_type;

  /// Open mode: `in`, `out` (can be combined using "bitwise or").
  /** According to the C++ standard, these should be in `std::ios_base`.  We
   * take them from derived class `std::ios` instead, which is easier on the
   * eyes.
   *
   * Historical note: taking it from std::ios was originally a workaround for a
   * problem with gcc 2.95.
   */
  using openmode = std::ios::openmode;

  /// Default open mode: in, out, binary.
  static constexpr auto default_mode{
    std::ios::in | std::ios::out | std::ios::binary};

  /// Seek direction: `beg`, `cur`, `end`.
  using seekdir = std::ios::seekdir;

  /// Create new large object and open it.
  /**
   * @param t Backend transaction in which the object is to be created.
   * @param mode Access mode, defaults to ios_base::in | ios_base::out |
   * ios_base::binary.
   */
  [[deprecated("Use blob instead.")]] explicit largeobjectaccess(
    dbtransaction &t, openmode mode = default_mode);

  /// Open large object with given oid.
  /** Convert combination of a transaction and object identifier into a
   * large object identity.  Does not affect the database.
   * @param t Transaction in which the object is to be accessed.
   * @param o Object identifier for the given object.
   * @param mode Access mode, defaults to ios_base::in | ios_base::out |
   * ios_base::binary.
   */
  [[deprecated("Use blob instead.")]] largeobjectaccess(
    dbtransaction &t, oid o, openmode mode = default_mode);

  /// Open given large object.
  /** Open a large object with the given identity for reading and/or writing.
   * @param t Transaction in which the object is to be accessed.
   * @param o Identity for the large object to be accessed.
   * @param mode Access mode, defaults to ios_base::in | ios_base::out |
   * ios_base::binary.
   */
  [[deprecated("Use blob instead.")]] largeobjectaccess(
    dbtransaction &t, largeobject o, openmode mode = default_mode);

  /// Import large object from a local file and open it.
  /** Creates a large object containing the data found in the given file.
   * @param t Backend transaction in which the large object is to be created.
   * @param file A filename on the client program's filesystem.
   * @param mode Access mode, defaults to ios_base::in | ios_base::out.
   */
  [[deprecated("Use blob instead.")]] largeobjectaccess(
    dbtransaction &t, std::string_view file, openmode mode = default_mode);

  ~largeobjectaccess() noexcept { close(); }

  /// Object identifier.
  /** The number returned by this function uniquely identifies the large object
   * in the context of the database we're connected to.
   */
  using largeobject::id;

  /// Export large object's contents to a local file.
  /** Writes the data stored in the large object to the given file.
   * @param file A filename on the client's filesystem.
   */
  void to_file(std::string_view file) const
  {
    largeobject::to_file(m_trans, file);
  }

  using largeobject::to_file;

  /**
   * @name High-level access to object contents.
   */
  //@{
  /// Write data to large object.
  /** @warning The size of a write is currently limited to 2GB.
   *
   * @param buf Data to write.
   * @param len Number of bytes from Buf to write.
   */
  void write(char const buf[], std::size_t len);

  /// Write string to large object.
  /** If not all bytes could be written, an exception is thrown.
   * @param buf Data to write; no terminating zero is written.
   */
  void write(std::string_view buf) { write(std::data(buf), std::size(buf)); }

  /// Read data from large object.
  /** Throws an exception if an error occurs while reading.
   * @param buf Location to store the read data in.
   * @param len Number of bytes to try and read.
   * @return Number of bytes read, which may be less than the number requested
   * if the end of the large object is reached.
   */
  size_type read(char buf[], std::size_t len);

  /// Seek in large object's data stream.
  /** Throws an exception if an error occurs.
   * @return The new position in the large object
   */
  size_type seek(size_type dest, seekdir dir);

  /// Report current position in large object's data stream.
  /** Throws an exception if an error occurs.
   * @return The current position in the large object.
   */
  [[nodiscard]] size_type tell() const;
  //@}

  /**
   * @name Low-level access to object contents.
   *
   * These functions provide a more "C-like" access interface, returning
   * special values instead of throwing exceptions on error.  These functions
   * are generally best avoided in favour of the high-level access functions,
   * which behave more like C++ functions should.
   *
   * Due to libpq's underlying API, some operations are limited to "int"
   * sizes, typically 2 GB, even though a large object can grow much larger.
   */
  //@{
  /// Seek in large object's data stream.
  /** Does not throw exception in case of error; inspect return value and
   * `errno` instead.
   * @param dest Offset to go to.
   * @param dir Origin to which dest is relative: ios_base::beg (from beginning
   *        of the object), ios_base::cur (from current access position), or
   *        ios_base;:end (from end of object).
   * @return New position in large object, or -1 if an error occurred.
   */
  pos_type cseek(off_type dest, seekdir dir) noexcept;

  /// Write to large object's data stream.
  /** Does not throw exception in case of error; inspect return value and
   * `errno` instead.
   * @param buf Data to write.
   * @param len Number of bytes to write.
   * @return Number of bytes actually written, or -1 if an error occurred.
   */
  off_type cwrite(char const buf[], std::size_t len) noexcept;

  /// Read from large object's data stream.
  /** Does not throw exception in case of error; inspect return value and
   * `errno` instead.
   * @param buf Area where incoming bytes should be stored.
   * @param len Number of bytes to read.
   * @return Number of bytes actually read, or -1 if an error occurred..
   */
  off_type cread(char buf[], std::size_t len) noexcept;

  /// Report current position in large object's data stream.
  /** Does not throw exception in case of error; inspect return value and
   * `errno` instead.
   * @return Current position in large object, of -1 if an error occurred.
   */
  [[nodiscard]] pos_type ctell() const noexcept;
  //@}

  /**
   * @name Error/warning output
   */
  //@{
  /// Issue message to transaction's notice processor.
  void process_notice(zview) noexcept;
  //@}

  using largeobject::remove;

  using largeobject::operator==;
  using largeobject::operator!=;
  using largeobject::operator<;
  using largeobject::operator<=;
  using largeobject::operator>;
  using largeobject::operator>=;

  largeobjectaccess() = delete;
  largeobjectaccess(largeobjectaccess const &) = delete;
  largeobjectaccess operator=(largeobjectaccess const &) = delete;

private:
  PQXX_PRIVATE std::string reason(int err) const;
  internal::pq::PGconn *raw_connection() const
  {
    return largeobject::raw_connection(m_trans);
  }

  PQXX_PRIVATE void open(openmode mode);
  void close() noexcept;

  dbtransaction &m_trans;
  int m_fd = -1;
};


/// Streambuf to use large objects in standard I/O streams.
/** @deprecated Access large objects directly using the @ref blob class.
 *
 * The standard streambuf classes provide uniform access to data storage such
 * as files or string buffers, so they can be accessed using standard input or
 * output streams.  This streambuf implementation provided similar access to
 * large objects, so they could be read and written using the same stream
 * classes.
 *
 * This functionality was considered too fragile and complex, so it has been
 * replaced with a single, much simpler class.
 */
template<typename CHAR = char, typename TRAITS = std::char_traits<CHAR>>
class largeobject_streambuf : public std::basic_streambuf<CHAR, TRAITS>
{
  using size_type = largeobject::size_type;

public:
  using char_type = CHAR;
  using traits_type = TRAITS;
  using int_type = typename traits_type::int_type;
  using pos_type = typename traits_type::pos_type;
  using off_type = typename traits_type::off_type;
  using openmode = largeobjectaccess::openmode;
  using seekdir = largeobjectaccess::seekdir;

  /// Default open mode: in, out, binary.
  static constexpr auto default_mode{
    std::ios::in | std::ios::out | std::ios::binary};

#include "pqxx/internal/ignore-deprecated-pre.hxx"
  [[deprecated("Use blob instead.")]] largeobject_streambuf(
    dbtransaction &t, largeobject o, openmode mode = default_mode,
    size_type buf_size = 512) :
          m_bufsize{buf_size}, m_obj{t, o, mode}, m_g{nullptr}, m_p{nullptr}
  {
    initialize(mode);
  }
#include "pqxx/internal/ignore-deprecated-post.hxx"

  [[deprecated("Use blob instead.")]] largeobject_streambuf(
    dbtransaction &t, oid o, openmode mode = default_mode,
    size_type buf_size = 512) :
          m_bufsize{buf_size}, m_obj{t, o, mode}, m_g{nullptr}, m_p{nullptr}
  {
    initialize(mode);
  }

  virtual ~largeobject_streambuf() noexcept
  {
    delete[] m_p;
    delete[] m_g;
  }

  /// For use by large object stream classes.
  void process_notice(zview const &s) { m_obj.process_notice(s); }

protected:
  virtual int sync() override
  {
    // setg() sets eback, gptr, egptr.
    this->setg(this->eback(), this->eback(), this->egptr());
    return overflow(eof());
  }

  virtual pos_type seekoff(off_type offset, seekdir dir, openmode) override
  {
    return adjust_eof(m_obj.cseek(largeobjectaccess::off_type(offset), dir));
  }

  virtual pos_type seekpos(pos_type pos, openmode) override
  {
    largeobjectaccess::pos_type const newpos{
      m_obj.cseek(largeobjectaccess::off_type(pos), std::ios::beg)};
    return adjust_eof(newpos);
  }

  virtual int_type overflow(int_type ch) override
  {
    auto *const pp{this->pptr()};
    if (pp == nullptr)
      return eof();
    auto *const pb{this->pbase()};
    int_type res{0};

    if (pp > pb)
    {
      auto const write_sz{pp - pb};
      auto const written_sz{
        m_obj.cwrite(pb, static_cast<std::size_t>(pp - pb))};
      if (internal::cmp_less_equal(written_sz, 0))
        throw internal_error{
          "pqxx::largeobject: write failed "
          "(is transaction still valid on write or flush?), "
          "libpq reports error"};
      else if (write_sz != written_sz)
        throw internal_error{
          "pqxx::largeobject: write failed "
          "(is transaction still valid on write or flush?), " +
          std::to_string(written_sz) + "/" + std::to_string(write_sz) +
          " bytes written"};
      auto const out{adjust_eof(written_sz)};

      if constexpr (std::is_arithmetic_v<decltype(out)>)
        res = check_cast<int_type>(out, "largeobject position"sv);
      else
        res = int_type(out);
    }
    this->setp(m_p, m_p + m_bufsize);

    // Write that one more character, if it's there.
    if (ch != eof())
    {
      *this->pptr() = static_cast<char_type>(ch);
      this->pbump(1);
    }
    return res;
  }

  virtual int_type overflow() { return overflow(eof()); }

  virtual int_type underflow() override
  {
    if (this->gptr() == nullptr)
      return eof();
    auto *const eb{this->eback()};
    auto const res{adjust_eof(
      m_obj.cread(this->eback(), static_cast<std::size_t>(m_bufsize)))};
    this->setg(
      eb, eb, eb + (res == eof() ? 0 : static_cast<std::size_t>(res)));
    return (res == eof() or res == 0) ? eof() : traits_type::to_int_type(*eb);
  }

private:
  /// Shortcut for traits_type::eof().
  static int_type eof() { return traits_type::eof(); }

  /// Helper: change error position of -1 to EOF (probably a no-op).
  template<typename INTYPE> static std::streampos adjust_eof(INTYPE pos)
  {
    bool const at_eof{pos == -1};
    if constexpr (std::is_arithmetic_v<std::streampos>)
    {
      return check_cast<std::streampos>(
        (at_eof ? eof() : pos), "large object seek"sv);
    }
    else
    {
      return std::streampos(at_eof ? eof() : pos);
    }
  }

  void initialize(openmode mode)
  {
    if ((mode & std::ios::in) != 0)
    {
      m_g = new char_type[unsigned(m_bufsize)];
      this->setg(m_g, m_g, m_g);
    }
    if ((mode & std::ios::out) != 0)
    {
      m_p = new char_type[unsigned(m_bufsize)];
      this->setp(m_p, m_p + m_bufsize);
    }
  }

  size_type const m_bufsize;
  largeobjectaccess m_obj;

  /// Get & put buffers.
  char_type *m_g, *m_p;
};


/// Input stream that gets its data from a large object.
/** @deprecated Access large objects directly using the @ref blob class.
 *
 * This class worked like any other istream, but to read data from a large
 * object.  It supported all formatting and streaming operations of
 * `std::istream`.
 *
 * This functionality was considered too fragile and complex, so it has been
 * replaced with a single, much simpler class.
 */
template<typename CHAR = char, typename TRAITS = std::char_traits<CHAR>>
class basic_ilostream : public std::basic_istream<CHAR, TRAITS>
{
  using super = std::basic_istream<CHAR, TRAITS>;

public:
  using char_type = CHAR;
  using traits_type = TRAITS;
  using int_type = typename traits_type::int_type;
  using pos_type = typename traits_type::pos_type;
  using off_type = typename traits_type::off_type;

#include "pqxx/internal/ignore-deprecated-pre.hxx"
  /// Create a basic_ilostream.
  /**
   * @param t Transaction in which this stream is to exist.
   * @param o Large object to access.
   * @param buf_size Size of buffer to use internally (optional).
   */
  [[deprecated("Use blob instead.")]] basic_ilostream(
    dbtransaction &t, largeobject o, largeobject::size_type buf_size = 512) :
          super{nullptr},
          m_buf{t, o, std::ios::in | std::ios::binary, buf_size}
  {
    super::init(&m_buf);
  }
#include "pqxx/internal/ignore-deprecated-post.hxx"

  /// Create a basic_ilostream.
  /**
   * @param t Transaction in which this stream is to exist.
   * @param o Identifier of a large object to access.
   * @param buf_size Size of buffer to use internally (optional).
   */
  [[deprecated("Use blob instead.")]] basic_ilostream(
    dbtransaction &t, oid o, largeobject::size_type buf_size = 512) :
          super{nullptr},
          m_buf{t, o, std::ios::in | std::ios::binary, buf_size}
  {
    super::init(&m_buf);
  }

private:
  largeobject_streambuf<CHAR, TRAITS> m_buf;
};

using ilostream = basic_ilostream<char>;


/// Output stream that writes data back to a large object.
/** @deprecated Access large objects directly using the @ref blob class.
 *
 * This worked like any other ostream, but to write data to a large object.
 * It supported all formatting and streaming operations of `std::ostream`.
 *
 * This functionality was considered too fragile and complex, so it has been
 * replaced with a single, much simpler class.
 */
template<typename CHAR = char, typename TRAITS = std::char_traits<CHAR>>
class basic_olostream : public std::basic_ostream<CHAR, TRAITS>
{
  using super = std::basic_ostream<CHAR, TRAITS>;

public:
  using char_type = CHAR;
  using traits_type = TRAITS;
  using int_type = typename traits_type::int_type;
  using pos_type = typename traits_type::pos_type;
  using off_type = typename traits_type::off_type;

#include "pqxx/internal/ignore-deprecated-pre.hxx"
  /// Create a basic_olostream.
  /**
   * @param t transaction in which this stream is to exist.
   * @param o a large object to access.
   * @param buf_size size of buffer to use internally (optional).
   */
  [[deprecated("Use blob instead.")]] basic_olostream(
    dbtransaction &t, largeobject o, largeobject::size_type buf_size = 512) :
          super{nullptr},
          m_buf{t, o, std::ios::out | std::ios::binary, buf_size}
  {
    super::init(&m_buf);
  }
#include "pqxx/internal/ignore-deprecated-post.hxx"

  /// Create a basic_olostream.
  /**
   * @param t transaction in which this stream is to exist.
   * @param o a large object to access.
   * @param buf_size size of buffer to use internally (optional).
   */
  [[deprecated("Use blob instead.")]] basic_olostream(
    dbtransaction &t, oid o, largeobject::size_type buf_size = 512) :
          super{nullptr},
          m_buf{t, o, std::ios::out | std::ios::binary, buf_size}
  {
    super::init(&m_buf);
  }

  ~basic_olostream()
  {
    try
    {
      m_buf.pubsync();
      m_buf.pubsync();
    }
    catch (std::exception const &e)
    {
      m_buf.process_notice(e.what());
    }
  }

private:
  largeobject_streambuf<CHAR, TRAITS> m_buf;
};

using olostream = basic_olostream<char>;


/// Stream that reads and writes a large object.
/** @deprecated Access large objects directly using the @ref blob class.
 *
 * This worked like a std::iostream, but to read data from, or write data to, a
 * large object.  It supported all formatting and streaming operations of
 * `std::iostream`.
 *
 * This functionality was considered too fragile and complex, so it has been
 * replaced with a single, much simpler class.
 */
template<typename CHAR = char, typename TRAITS = std::char_traits<CHAR>>
class basic_lostream : public std::basic_iostream<CHAR, TRAITS>
{
  using super = std::basic_iostream<CHAR, TRAITS>;

public:
  using char_type = CHAR;
  using traits_type = TRAITS;
  using int_type = typename traits_type::int_type;
  using pos_type = typename traits_type::pos_type;
  using off_type = typename traits_type::off_type;

  /// Create a basic_lostream.
  /**
   * @param t Transaction in which this stream is to exist.
   * @param o Large object to access.
   * @param buf_size Size of buffer to use internally (optional).
   */
  [[deprecated("Use blob instead.")]] basic_lostream(
    dbtransaction &t, largeobject o, largeobject::size_type buf_size = 512) :
          super{nullptr},
          m_buf{
            t, o, std::ios::in | std::ios::out | std::ios::binary, buf_size}
  {
    super::init(&m_buf);
  }

  /// Create a basic_lostream.
  /**
   * @param t Transaction in which this stream is to exist.
   * @param o Large object to access.
   * @param buf_size Size of buffer to use internally (optional).
   */
  [[deprecated("Use blob instead.")]] basic_lostream(
    dbtransaction &t, oid o, largeobject::size_type buf_size = 512) :
          super{nullptr},
          m_buf{
            t, o, std::ios::in | std::ios::out | std::ios::binary, buf_size}
  {
    super::init(&m_buf);
  }

  ~basic_lostream()
  {
    try
    {
      m_buf.pubsync();
      m_buf.pubsync();
    }
    catch (std::exception const &e)
    {
      m_buf.process_notice(e.what());
    }
  }

private:
  largeobject_streambuf<CHAR, TRAITS> m_buf;
};

using lostream = basic_lostream<char>;
} // namespace pqxx
#endif
