/* Binary Large Objects interface.
 *
 * Read or write large objects, stored in their own storage on the server.
 *
 * DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/largeobject instead.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#ifndef PQXX_H_BLOB
#define PQXX_H_BLOB

#if !defined(PQXX_HEADER_PRE)
#  error "Include libpqxx headers as <pqxx/header>, not <pqxx/header.hxx>."
#endif

#include <cstdint>

#if defined(PQXX_HAVE_PATH)
#  include <filesystem>
#endif

#if defined(PQXX_HAVE_RANGES) && __has_include(<ranges>)
#  include <ranges>
#endif

#if defined(PQXX_HAVE_SPAN) && __has_include(<span>)
#  include <span>
#endif

#include "pqxx/dbtransaction.hxx"


namespace pqxx
{
/** Binary large object.
 *
 * This is how you store data that may be too large for the `BYTEA` type.
 * Access operations are similar to those for a file: you can read, write,
 * query or set the current reading/writing position, and so on.
 *
 * These large objects live in their own storage on the server, indexed by an
 * integer object identifier ("oid").
 *
 * Two `blob` objects may refer to the same actual large object in the
 * database at the same time.  Each will have its own reading/writing position,
 * but writes to the one will of course affect what the other sees.
 */
class PQXX_LIBEXPORT blob
{
public:
  /// Create a new, empty large object.
  /** You may optionally specify an oid for the new blob.  If you do, then
   * the new object will have that oid -- or creation will fail if there
   * already is an object with that oid.
   */
  [[nodiscard]] static oid create(dbtransaction &, oid = 0);

  /// Delete a large object, or fail if it does not exist.
  static void remove(dbtransaction &, oid);

  /// Open blob for reading.  Any attempt to write to it will fail.
  [[nodiscard]] static blob open_r(dbtransaction &, oid);
  // Open blob for writing.  Any attempt to read from it will fail.
  [[nodiscard]] static blob open_w(dbtransaction &, oid);
  // Open blob for reading and/or writing.
  [[nodiscard]] static blob open_rw(dbtransaction &, oid);

  /// You can default-construct a blob, but it won't do anything useful.
  /** Most operations on a default-constructed blob will throw @ref
   * usage_error.
   */
  blob() = default;

  /// You can move a blob, but not copy it.  The original becomes unusable.
  blob(blob &&);
  /// You can move a blob, but not copy it.  The original becomes unusable.
  blob &operator=(blob &&);

  blob(blob const &) = delete;
  blob &operator=(blob const &) = delete;
  ~blob();

  /// Maximum number of bytes that can be read or written at a time.
  /** The underlying protocol only supports reads and writes up to 2 GB
   * exclusive.
   *
   * If you need to read or write more data to or from a binary large object,
   * you'll have to break it up into chunks.
   */
  static constexpr std::size_t chunk_limit = 0x7fffffff;

  /// Read up to `size` bytes of the object into `buf`.
  /** Uses a buffer that you provide, resizing it as needed.  If it suits you,
   * this lets you allocate the buffer once and then re-use it multiple times.
   *
   * Resizes `buf` as needed.
   *
   * @warning The underlying protocol only supports reads up to 2GB at a time.
   * If you need to read more, try making repeated calls to @ref append_to_buf.
   */
  std::size_t read(std::basic_string<std::byte> &buf, std::size_t size);

#if defined(PQXX_HAVE_SPAN)
  /// Read up to `std::size(buf)` bytes from the object.
  /** Retrieves bytes from the blob, at the current position, until `buf` is
   * full or there are no more bytes to read, whichever comes first.
   *
   * Returns the filled portion of `buf`.  This may be empty.
   */
  template<std::size_t extent = std::dynamic_extent>
  std::span<std::byte> read(std::span<std::byte, extent> buf)
  {
    return buf.subspan(0, raw_read(std::data(buf), std::size(buf)));
  }
#endif // PQXX_HAVE_SPAN

#if defined(PQXX_HAVE_CONCEPTS) && defined(PQXX_HAVE_SPAN)
  /// Read up to `std::size(buf)` bytes from the object.
  /** Retrieves bytes from the blob, at the current position, until `buf` is
   * full or there are no more bytes to read, whichever comes first.
   *
   * Returns the filled portion of `buf`.  This may be empty.
   */
  template<binary DATA> std::span<std::byte> read(DATA &buf)
  {
    return {std::data(buf), raw_read(std::data(buf), std::size(buf))};
  }
#else  // PQXX_HAVE_CONCEPTS && PQXX_HAVE_SPAN
  /// Read up to `std::size(buf)` bytes from the object.
  /** @deprecated As libpqxx moves to C++20 as its baseline language version,
   * this will take and return `std::span<std::byte>`.
   *
   * Retrieves bytes from the blob, at the current position, until `buf` is
   * full (i.e. its current size is reached), or there are no more bytes to
   * read, whichever comes first.
   *
   * This function will not change either the size or the capacity of `buf`,
   * only its contents.
   *
   * Returns the filled portion of `buf`.  This may be empty.
   */
  template<typename ALLOC>
  std::basic_string_view<std::byte> read(std::vector<std::byte, ALLOC> &buf)
  {
    return {std::data(buf), raw_read(std::data(buf), std::size(buf))};
  }
#endif // PQXX_HAVE_CONCEPTS && PQXX_HAVE_SPAN

#if defined(PQXX_HAVE_CONCEPTS)
  /// Write `data` to large object, at the current position.
  /** If the writing position is at the end of the object, this will append
   * `data` to the object's contents and move the writing position so that
   * it's still at the end.
   *
   * If the writing position was not at the end, writing will overwrite the
   * prior data, but it will not remove data that follows the part where you
   * wrote your new data.
   *
   * @warning This is a big difference from writing to a file.  You can
   * overwrite some data in a large object, but this does not truncate the
   * data that was already there.  For example, if the object contained binary
   * data "abc", and you write "12" at the starting position, the object will
   * contain "12c".
   *
   * @warning The underlying protocol only supports writes up to 2 GB at a
   * time.  If you need to write more, try making repeated calls to
   * @ref append_from_buf.
   */
  template<binary DATA> void write(DATA const &data)
  {
    raw_write(std::data(data), std::size(data));
  }
#else
  /// Write `data` large object, at the current position.
  /** If the writing position is at the end of the object, this will append
   * `data` to the object's contents and move the writing position so that
   * it's still at the end.
   *
   * If the writing position was not at the end, writing will overwrite the
   * prior data, but it will not remove data that follows the part where you
   * wrote your new data.
   *
   * @warning This is a big difference from writing to a file.  You can
   * overwrite some data in a large object, but this does not truncate the
   * data that was already there.  For example, if the object contained binary
   * data "abc", and you write "12" at the starting position, the object will
   * contain "12c".
   *
   * @warning The underlying protocol only supports writes up to 2 GB at a
   * time.  If you need to write more, try making repeated calls to
   * @ref append_from_buf.
   */
  template<typename DATA> void write(DATA const &data)
  {
    raw_write(std::data(data), std::size(data));
  }
#endif

  /// Resize large object to `size` bytes.
  /** If the blob is more than `size` bytes long, this removes the end so as
   * to make the blob the desired length.
   *
   * If the blob is less than `size` bytes long, it adds enough zero bytes to
   * make it the desired length.
   */
  void resize(std::int64_t size);

  /// Return the current reading/writing position in the large object.
  [[nodiscard]] std::int64_t tell() const;

  /// Set the current reading/writing position to an absolute offset.
  /** Returns the new file offset. */
  std::int64_t seek_abs(std::int64_t offset = 0);
  /// Move the current reading/writing position forwards by an offset.
  /** To move backwards, pass a negative offset.
   *
   * Returns the new file offset.
   */
  std::int64_t seek_rel(std::int64_t offset = 0);
  /// Set the current position to an offset relative to the end of the blob.
  /** You'll probably want an offset of zero or less.
   *
   * Returns the new file offset.
   */
  std::int64_t seek_end(std::int64_t offset = 0);

  /// Create a binary large object containing given `data`.
  /** You may optionally specify an oid for the new object.  If you do, and an
   * object with that oid already exists, creation will fail.
   */
  static oid from_buf(
    dbtransaction &tx, std::basic_string_view<std::byte> data, oid id = 0);

  /// Append `data` to binary large object.
  /** The underlying protocol only supports appending blocks up to 2 GB.
   */
  static void append_from_buf(
    dbtransaction &tx, std::basic_string_view<std::byte> data, oid id);

  /// Read client-side file and store it server-side as a binary large object.
  [[nodiscard]] static oid from_file(dbtransaction &, char const path[]);

#if defined(PQXX_HAVE_PATH) && !defined(_WIN32)
  /// Read client-side file and store it server-side as a binary large object.
  /** This overload is not available on Windows, where `std::filesystem::path`
   * converts to a `wchar_t` string rather than a `char` string.
   */
  [[nodiscard]] static oid
  from_file(dbtransaction &tx, std::filesystem::path const &path)
  {
    return from_file(tx, path.c_str());
  }
#endif

  /// Read client-side file and store it server-side as a binary large object.
  /** In this version, you specify the binary large object's oid.  If that oid
   * is already in use, the operation will fail.
   */
  static oid from_file(dbtransaction &, char const path[], oid);

#if defined(PQXX_HAVE_PATH) && !defined(_WIN32)
  /// Read client-side file and store it server-side as a binary large object.
  /** In this version, you specify the binary large object's oid.  If that oid
   * is already in use, the operation will fail.
   *
   * This overload is not available on Windows, where `std::filesystem::path`
   * converts to a `wchar_t` string rather than a `char` string.
   */
  static oid
  from_file(dbtransaction &tx, std::filesystem::path const &path, oid id)
  {
    return from_file(tx, path.c_str(), id);
  }
#endif

  /// Convenience function: Read up to `max_size` bytes from blob with `id`.
  /** You could easily do this yourself using the @ref open_r and @ref read
   * functions, but it can save you a bit of code to do it this way.
   */
  static void to_buf(
    dbtransaction &, oid, std::basic_string<std::byte> &,
    std::size_t max_size);

  /// Read part of the binary large object with `id`, and append it to `buf`.
  /** Use this to break up a large read from one binary large object into one
   * massive buffer.  Just keep calling this function until it returns zero.
   *
   * The `offset` is how far into the large object your desired chunk is, and
   * `append_max` says how much to try and read in one go.
   */
  static std::size_t append_to_buf(
    dbtransaction &tx, oid id, std::int64_t offset,
    std::basic_string<std::byte> &buf, std::size_t append_max);

  /// Write a binary large object's contents to a client-side file.
  static void to_file(dbtransaction &, oid, char const path[]);

#if defined(PQXX_HAVE_PATH) && !defined(_WIN32)
  /// Write a binary large object's contents to a client-side file.
  /** This overload is not available on Windows, where `std::filesystem::path`
   * converts to a `wchar_t` string rather than a `char` string.
   */
  static void
  to_file(dbtransaction &tx, oid id, std::filesystem::path const &path)
  {
    to_file(tx, id, path.c_str());
  }
#endif

  /// Close this blob.
  /** This does not delete the blob from the database; it only terminates your
   * local object for accessing the blob.
   *
   * Resets the blob to a useless state similar to one that was
   * default-constructed.
   *
   * The destructor will do this for you automatically.  Still, there is a
   * reason to `close()` objects explicitly where possible: if an error should
   * occur while closing, `close()` can throw an exception.  A destructor
   * cannot.
   */
  void close();

private:
  PQXX_PRIVATE blob(connection &conn, int fd) noexcept :
          m_conn{&conn}, m_fd{fd}
  {}
  static PQXX_PRIVATE blob open_internal(dbtransaction &, oid, int);
  static PQXX_PRIVATE pqxx::internal::pq::PGconn *
  raw_conn(pqxx::connection *) noexcept;
  static PQXX_PRIVATE pqxx::internal::pq::PGconn *
  raw_conn(pqxx::dbtransaction const &) noexcept;
  static PQXX_PRIVATE std::string errmsg(connection const *);
  static PQXX_PRIVATE std::string errmsg(dbtransaction const &tx)
  {
    return errmsg(&tx.conn());
  }
  PQXX_PRIVATE std::string errmsg() const { return errmsg(m_conn); }
  PQXX_PRIVATE std::int64_t seek(std::int64_t offset, int whence);
  std::size_t raw_read(std::byte buf[], std::size_t size);
  void raw_write(std::byte const buf[], std::size_t size);

  connection *m_conn = nullptr;
  int m_fd = -1;
};
} // namespace pqxx
#endif
