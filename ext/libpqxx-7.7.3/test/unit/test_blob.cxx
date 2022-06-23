#include <cstdio>

#include <pqxx/blob>
#include <pqxx/transaction>

#include "../test_helpers.hxx"
#include "../test_types.hxx"


namespace
{
void test_blob_is_useless_by_default()
{
  pqxx::blob b{};
  std::basic_string<std::byte> buf;
  PQXX_CHECK_THROWS(
    b.read(buf, 1), pqxx::usage_error,
    "Read on default-constructed blob did not throw failure.");
  PQXX_CHECK_THROWS(
    b.write(buf), pqxx::usage_error,
    "Write on default-constructed blob did not throw failure.");
}


void test_blob_create_makes_empty_blob()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::oid id{pqxx::blob::create(tx)};
  auto b{pqxx::blob::open_r(tx, id)};
  b.seek_end(0);
  PQXX_CHECK_EQUAL(b.tell(), 0, "New blob is not empty.");
}


void test_blob_create_with_oid_requires_oid_be_free()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{pqxx::blob::create(tx)};

  PQXX_CHECK_THROWS(
    pqxx::ignore_unused(pqxx::blob::create(tx, id)), pqxx::failure,
    "Not getting expected error when oid not free.");
}


void test_blob_create_with_oid_obeys_oid()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{pqxx::blob::create(tx)};
  pqxx::blob::remove(tx, id);

  auto actual_id{pqxx::blob::create(tx, id)};
  PQXX_CHECK_EQUAL(actual_id, id, "Create with oid returned different oid.");
}


void test_blobs_are_transactional()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::oid id{pqxx::blob::create(tx)};
  tx.abort();
  pqxx::work tx2{conn};
  PQXX_CHECK_THROWS(
    pqxx::ignore_unused(pqxx::blob::open_r(tx2, id)), pqxx::failure,
    "Blob from aborted transaction still exists.");
}


void test_blob_remove_removes_blob()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::oid id{pqxx::blob::create(tx)};
  pqxx::blob::remove(tx, id);
  PQXX_CHECK_THROWS(
    pqxx::ignore_unused(pqxx::blob::open_r(tx, id)), pqxx::failure,
    "Attempt to open blob after removing should have failed.");
}


void test_blob_remove_is_not_idempotent()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::oid id{pqxx::blob::create(tx)};
  pqxx::blob::remove(tx, id);
  PQXX_CHECK_THROWS(
    pqxx::blob::remove(tx, id), pqxx::failure,
    "Redundant remove() did not throw failure.");
}


void test_blob_checks_open_mode()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::oid id{pqxx::blob::create(tx)};
  pqxx::blob b_r{pqxx::blob::open_r(tx, id)};
  pqxx::blob b_w{pqxx::blob::open_w(tx, id)};
  pqxx::blob b_rw{pqxx::blob::open_rw(tx, id)};

  std::basic_string<std::byte> buf{std::byte{3}, std::byte{2}, std::byte{1}};

  // These are all allowed:
  b_w.write(buf);
  b_r.read(buf, 3);
  b_rw.seek_end(0);
  b_rw.write(buf);
  b_rw.seek_abs(0);
  b_rw.read(buf, 6);

  // These are not:
  PQXX_CHECK_THROWS(
    b_r.write(buf), pqxx::failure, "Read-only blob did not stop write.");
  PQXX_CHECK_THROWS(
    b_w.read(buf, 10), pqxx::failure, "Write-only blob did not stop read.");
}


void test_blob_supports_move()
{
  std::basic_string<std::byte> buf;
  buf.push_back(std::byte{'x'});

  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::oid id{pqxx::blob::create(tx)};
  pqxx::blob b1{pqxx::blob::open_rw(tx, id)};
  b1.write(buf);

  pqxx::blob b2{std::move(b1)};
  b2.seek_abs(0);
  b2.read(buf, 1u);

  PQXX_CHECK_THROWS(
    b1.read(buf, 1u), pqxx::usage_error,
    "Blob still works after move construction.");

  b1 = std::move(b2);
  b1.read(buf, 1u);

  PQXX_CHECK_THROWS(
    b2.read(buf, 1u), pqxx::usage_error,
    "Blob still works after move assignment.");
}


void test_blob_read_reads_data()
{
  std::basic_string<std::byte> const data{
    std::byte{'a'}, std::byte{'b'}, std::byte{'c'}};

  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::oid id{pqxx::blob::from_buf(tx, data)};

  std::basic_string<std::byte> buf;
  auto b{pqxx::blob::open_rw(tx, id)};
  PQXX_CHECK_EQUAL(
    b.read(buf, 2), 2u, "Full read() returned an unexpected value.");
  PQXX_CHECK_EQUAL(
    buf, (std::basic_string<std::byte>{std::byte{'a'}, std::byte{'b'}}),
    "Read back the wrong data.");
  PQXX_CHECK_EQUAL(
    b.read(buf, 2), 1u, "Partial read() returned an unexpected value.");
  PQXX_CHECK_EQUAL(
    buf, (std::basic_string<std::byte>{std::byte{'c'}}),
    "Continued read produced wrong data.");
  PQXX_CHECK_EQUAL(
    b.read(buf, 2), 0u, "read at end returned an unexpected value.");
  PQXX_CHECK_EQUAL(
    buf, (std::basic_string<std::byte>{}), "Read past end produced data.");
}


void test_blob_read_span()
{
#if defined(PQXX_HAVE_SPAN)
  std::basic_string<std::byte> const data{std::byte{'u'}, std::byte{'v'},
                                          std::byte{'w'}, std::byte{'x'},
                                          std::byte{'y'}, std::byte{'z'}};

  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::oid id{pqxx::blob::from_buf(tx, data)};

  auto b{pqxx::blob::open_r(tx, id)};
  std::basic_string<std::byte> string_buf;
  string_buf.resize(2);

  std::span<std::byte> output;

  output = b.read(std::span<std::byte>{});
  PQXX_CHECK_EQUAL(
    std::size(output), 0u, "Empty read produced nonempty buffer.");
  output = b.read(string_buf);
  PQXX_CHECK_EQUAL(
    std::size(output), 2u, "Got unexpected buf size from blob::read().");
  PQXX_CHECK_EQUAL(
    output[0], std::byte{'u'}, "Unexpected byte from blob::read().");
  PQXX_CHECK_EQUAL(
    output[1], std::byte{'v'}, "Unexpected byte from blob::read().");

  string_buf.resize(100);
  output = b.read(std::span<std::byte>{string_buf.data(), 1});
  PQXX_CHECK_EQUAL(
    std::size(output), 1u,
    "Did blob::read() follow string size instead of span size?");
  PQXX_CHECK_EQUAL(
    output[0], std::byte{'w'}, "Unexpected byte from blob::read().");

  std::vector<std::byte> vec_buf;
  vec_buf.resize(2);
  auto output2{b.read(vec_buf)};
  PQXX_CHECK_EQUAL(
    std::size(output2), 2u, "Got unexpected buf size from blob::read().");
  PQXX_CHECK_EQUAL(
    output2[0], std::byte{'x'}, "Unexpected byte from blob::read().");
  PQXX_CHECK_EQUAL(
    output2[1], std::byte{'y'}, "Unexpected byte from blob::read().");

  vec_buf.resize(100);
  output2 = b.read(vec_buf);
  PQXX_CHECK_EQUAL(std::size(output2), 1u, "Weird things happened at EOF.");
  PQXX_CHECK_EQUAL(output2[0], std::byte{'z'}, "Bad data at EOF.");
#endif // PQXX_HAVE_SPAN
}


void test_blob_reads_vector()
{
  char const content[]{"abcd"};
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{pqxx::blob::from_buf(
    tx, std::basic_string_view<std::byte>{
          reinterpret_cast<std::byte const *>(content), std::size(content)})};
  std::vector<std::byte> buf;
  buf.resize(10);
  auto out{pqxx::blob::open_r(tx, id).read(buf)};
  PQXX_CHECK_EQUAL(
    std::size(out), std::size(content),
    "Got wrong length back when reading as vector.");
  PQXX_CHECK_EQUAL(
    out[0], std::byte{'a'}, "Got bad data when reading as vector.");
}


void test_blob_write_appends_at_insertion_point()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{pqxx::blob::create(tx)};

  auto b{pqxx::blob::open_rw(tx, id)};
  b.write(std::basic_string<std::byte>{std::byte{'z'}});
  b.write(std::basic_string<std::byte>{std::byte{'a'}});

  std::basic_string<std::byte> buf;
  b.read(buf, 5);
  PQXX_CHECK_EQUAL(
    buf, (std::basic_string<std::byte>{}), "Found data at the end.");
  b.seek_abs(0);
  b.read(buf, 5);
  PQXX_CHECK_EQUAL(
    buf, (std::basic_string<std::byte>{std::byte{'z'}, std::byte{'a'}}),
    "Consecutive writes did not append correctly.");

  b.write(std::basic_string<std::byte>{std::byte{'x'}});
  // Blob now contains "zax".  That's not we wanted...  Rewind and rewrite.
  b.seek_abs(1);
  b.write(std::basic_string<std::byte>{std::byte{'y'}});
  b.seek_abs(0);
  b.read(buf, 5);
  PQXX_CHECK_EQUAL(
    buf,
    (std::basic_string<std::byte>{
      std::byte{'z'}, std::byte{'y'}, std::byte{'x'}}),
    "Rewriting in the middle did not work right.");
}


void test_blob_writes_span()
{
#if defined(PQXX_HAVE_SPAN)
  pqxx::connection conn;
  pqxx::work tx{conn};
  constexpr char content[]{"gfbltk"};
  std::basic_string<std::byte> data{
    reinterpret_cast<std::byte const *>(content), std::size(content)};

  auto id{pqxx::blob::create(tx)};
  auto b{pqxx::blob::open_rw(tx, id)};
  b.write(std::span<std::byte>{data.data() + 1, 3u});
  b.seek_abs(0);

  std::vector<std::byte> buf;
  buf.resize(4);
  auto out{b.read(std::span<std::byte>{buf.data(), 4u})};
  PQXX_CHECK_EQUAL(
    std::size(out), 3u, "Did not get expected number of bytes back.");
  PQXX_CHECK_EQUAL(out[0], std::byte{'f'}, "Data did not come back right.");
  PQXX_CHECK_EQUAL(out[2], std::byte{'l'}, "Data started right, ended wrong!");
#endif // PQXX_HAVE_SPAN
}


void test_blob_resize_shortens_to_desired_length()
{
  std::basic_string<std::byte> const data{
    std::byte{'w'}, std::byte{'o'}, std::byte{'r'}, std::byte{'k'}};

  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{pqxx::blob::from_buf(tx, data)};

  pqxx::blob::open_w(tx, id).resize(2);
  std::basic_string<std::byte> buf;
  pqxx::blob::to_buf(tx, id, buf, 10);
  PQXX_CHECK_EQUAL(
    buf, (std::basic_string<std::byte>{std::byte{'w'}, std::byte{'o'}}),
    "Truncate did not shorten correctly.");
}


void test_blob_resize_extends_to_desired_length()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{
    pqxx::blob::from_buf(tx, std::basic_string<std::byte>{std::byte{100}})};
  pqxx::blob::open_w(tx, id).resize(3);
  std::basic_string<std::byte> buf;
  pqxx::blob::to_buf(tx, id, buf, 10);
  PQXX_CHECK_EQUAL(
    buf,
    (std::basic_string<std::byte>{std::byte{100}, std::byte{0}, std::byte{0}}),
    "Resize did not zero-extend correctly.");
}


void test_blob_tell_tracks_position()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{pqxx::blob::create(tx)};
  auto b{pqxx::blob::open_rw(tx, id)};

  PQXX_CHECK_EQUAL(
    b.tell(), 0, "Empty blob started out in non-zero position.");
  b.write(std::basic_string<std::byte>{std::byte{'e'}, std::byte{'f'}});
  PQXX_CHECK_EQUAL(
    b.tell(), 2, "Empty blob started out in non-zero position.");
  b.seek_abs(1);
  PQXX_CHECK_EQUAL(b.tell(), 1, "tell() did not track seek.");
}


void test_blob_seek_sets_positions()
{
  std::basic_string<std::byte> data{
    std::byte{0}, std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
    std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}, std::byte{9}};
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{pqxx::blob::from_buf(tx, data)};
  auto b{pqxx::blob::open_r(tx, id)};

  std::basic_string<std::byte> buf;
  b.seek_rel(3);
  b.read(buf, 1u);
  PQXX_CHECK_EQUAL(
    buf[0], std::byte{3},
    "seek_rel() from beginning did not take us to the right position.");

  b.seek_abs(2);
  b.read(buf, 1u);
  PQXX_CHECK_EQUAL(
    buf[0], std::byte{2}, "seek_abs() did not take us to the right position.");

  b.seek_end(-2);
  b.read(buf, 1u);
  PQXX_CHECK_EQUAL(
    buf[0], std::byte{8}, "seek_end() did not take us to the right position.");
}


void test_blob_from_buf_interoperates_with_to_buf()
{
  std::basic_string<std::byte> const data{std::byte{'h'}, std::byte{'i'}};
  std::basic_string<std::byte> buf;
  pqxx::connection conn;
  pqxx::work tx{conn};
  pqxx::blob::to_buf(tx, pqxx::blob::from_buf(tx, data), buf, 10);
  PQXX_CHECK_EQUAL(buf, data, "from_buf()/to_buf() roundtrip did not work.");
}


void test_blob_append_from_buf_appends()
{
  std::basic_string<std::byte> const data{std::byte{'h'}, std::byte{'o'}};
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{pqxx::blob::create(tx)};
  pqxx::blob::append_from_buf(tx, data, id);
  pqxx::blob::append_from_buf(tx, data, id);
  std::basic_string<std::byte> buf;
  pqxx::blob::to_buf(tx, id, buf, 10);
  PQXX_CHECK_EQUAL(buf, data + data, "append_from_buf() wrote wrong data?");
}


namespace
{
/// Wrap `std::fopen`.
/** This is just here to stop Visual Studio from advertising its own
 * alternative.
 */
std::unique_ptr<FILE, std::function<int(FILE *)>>
my_fopen(char const *path, char const *mode)
{
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4996)
#endif
  return {std::fopen(path, mode), std::fclose};
#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
}


void read_file(
  char const path[], std::size_t len, std::basic_string<std::byte> &buf)
{
  buf.resize(len);
  auto f{my_fopen(path, "rb")};
  auto bytes{
    std::fread(reinterpret_cast<char *>(buf.data()), 1, len, f.get())};
  if (bytes == 0)
    throw std::runtime_error{"Error reading test file."};
  buf.resize(bytes);
}


void write_file(char const path[], std::basic_string_view<std::byte> data)
{
  try
  {
    auto f{my_fopen(path, "wb")};
    if (
      std::fwrite(
        reinterpret_cast<char const *>(data.data()), 1, std::size(data),
        f.get()) < std::size(data))
      std::runtime_error{"File write failed."};
  }
  catch (const std::exception &)
  {
    std::remove(path);
    throw;
  }
}


/// Temporary file.
class TempFile
{
public:
  /// Create (and later clean up) a file at path containing data.
  TempFile(char const path[], std::basic_string_view<std::byte> data) :
          m_path(path)
  {
    write_file(path, data);
  }

  ~TempFile() { std::remove(m_path.c_str()); }

private:
  std::string m_path;
};
} // namespace


void test_blob_from_file_creates_blob_from_file_contents()
{
  char const temp_file[] = "blob-test-from_file.tmp";
  std::basic_string<std::byte> const data{std::byte{'4'}, std::byte{'2'}};

  pqxx::connection conn;
  pqxx::work tx{conn};
  std::basic_string<std::byte> buf;

  pqxx::oid id;
  {
    TempFile f{temp_file, data};
    id = pqxx::blob::from_file(tx, temp_file);
  }
  pqxx::blob::to_buf(tx, id, buf, 10);
  PQXX_CHECK_EQUAL(buf, data, "Wrong data from blob::from_file().");
}


void test_blob_from_file_with_oid_writes_blob()
{
  std::basic_string<std::byte> const data{std::byte{'6'}, std::byte{'9'}};
  char const temp_file[] = "blob-test-from_file-oid.tmp";
  std::basic_string<std::byte> buf;

  pqxx::connection conn;
  pqxx::work tx{conn};

  // Guarantee (more or less) that id is not in use.
  auto id{pqxx::blob::create(tx)};
  pqxx::blob::remove(tx, id);

  {
    TempFile f{temp_file, data};
    pqxx::blob::from_file(tx, temp_file, id);
  }
  pqxx::blob::to_buf(tx, id, buf, 10);
  PQXX_CHECK_EQUAL(buf, data, "Wrong data from blob::from_file().");
}


void test_blob_append_to_buf_appends()
{
  std::basic_string<std::byte> const data{
    std::byte{'b'}, std::byte{'l'}, std::byte{'u'}, std::byte{'b'}};

  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{pqxx::blob::from_buf(tx, data)};

  std::basic_string<std::byte> buf;
  PQXX_CHECK_EQUAL(
    pqxx::blob::append_to_buf(tx, id, 0u, buf, 1u), 1u,
    "append_to_buf() returned unexpected value.");
  PQXX_CHECK_EQUAL(std::size(buf), 1u, "Appended the wrong number of bytes.");
  PQXX_CHECK_EQUAL(
    pqxx::blob::append_to_buf(tx, id, 1u, buf, 5u), 3u,
    "append_to_buf() returned unexpected value.");
  PQXX_CHECK_EQUAL(std::size(buf), 4u, "Appended the wrong number of bytes.");

  PQXX_CHECK_EQUAL(
    buf, data, "Reading using append_to_buf gave us wrong data.");
}


void test_blob_to_file_writes_file()
{
  std::basic_string<std::byte> const data{
    std::byte{'C'}, std::byte{'+'}, std::byte{'+'}};

  char const temp_file[] = "blob-test-to_file.tmp";
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{pqxx::blob::from_buf(tx, data)};
  std::basic_string<std::byte> buf;

  try
  {
    pqxx::blob::to_file(tx, id, temp_file);
    read_file(temp_file, 10u, buf);
    std::remove(temp_file);
  }
  catch (std::exception const &)
  {
    std::remove(temp_file);
    throw;
  }
  PQXX_CHECK_EQUAL(buf, data, "Got wrong data from to_file().");
}


void test_blob_close_leaves_blob_unusable()
{
  pqxx::connection conn;
  pqxx::work tx{conn};
  auto id{
    pqxx::blob::from_buf(tx, std::basic_string<std::byte>{std::byte{1}})};
  auto b{pqxx::blob::open_rw(tx, id)};
  b.close();
  std::basic_string<std::byte> buf;
  PQXX_CHECK_THROWS(
    b.read(buf, 1), pqxx::usage_error,
    "Reading from closed blob did not fail right.");
}


void test_blob_accepts_std_filesystem_path()
{
#if defined(PQXX_HAVE_PATH) && !defined(_WIN32)
  // A bug in gcc 8's ~std::filesystem::path() causes a run-time crash.
#  if !defined(__GNUC__) || (__GNUC__ > 8)

  char const temp_file[] = "blob-test-filesystem-path.tmp";
  std::basic_string<std::byte> const data{std::byte{'4'}, std::byte{'2'}};

  pqxx::connection conn;
  pqxx::work tx{conn};
  std::basic_string<std::byte> buf;

  TempFile f{temp_file, data};
  std::filesystem::path const path{temp_file};
  auto id{pqxx::blob::from_file(tx, path)};
  pqxx::blob::to_buf(tx, id, buf, 10);
  PQXX_CHECK_EQUAL(buf, data, "Wrong data from blob::from_file().");

#  endif
#endif
}


PQXX_REGISTER_TEST(test_blob_is_useless_by_default);
PQXX_REGISTER_TEST(test_blob_create_makes_empty_blob);
PQXX_REGISTER_TEST(test_blob_create_with_oid_requires_oid_be_free);
PQXX_REGISTER_TEST(test_blob_create_with_oid_obeys_oid);
PQXX_REGISTER_TEST(test_blobs_are_transactional);
PQXX_REGISTER_TEST(test_blob_remove_removes_blob);
PQXX_REGISTER_TEST(test_blob_remove_is_not_idempotent);
PQXX_REGISTER_TEST(test_blob_checks_open_mode);
PQXX_REGISTER_TEST(test_blob_supports_move);
PQXX_REGISTER_TEST(test_blob_read_reads_data);
PQXX_REGISTER_TEST(test_blob_reads_vector);
PQXX_REGISTER_TEST(test_blob_read_span);
PQXX_REGISTER_TEST(test_blob_write_appends_at_insertion_point);
PQXX_REGISTER_TEST(test_blob_writes_span);
PQXX_REGISTER_TEST(test_blob_resize_shortens_to_desired_length);
PQXX_REGISTER_TEST(test_blob_resize_extends_to_desired_length);
PQXX_REGISTER_TEST(test_blob_tell_tracks_position);
PQXX_REGISTER_TEST(test_blob_seek_sets_positions);
PQXX_REGISTER_TEST(test_blob_from_buf_interoperates_with_to_buf);
PQXX_REGISTER_TEST(test_blob_append_from_buf_appends);
PQXX_REGISTER_TEST(test_blob_from_file_creates_blob_from_file_contents);
PQXX_REGISTER_TEST(test_blob_from_file_with_oid_writes_blob);
PQXX_REGISTER_TEST(test_blob_append_to_buf_appends);
PQXX_REGISTER_TEST(test_blob_to_file_writes_file);
PQXX_REGISTER_TEST(test_blob_close_leaves_blob_unusable);
PQXX_REGISTER_TEST(test_blob_accepts_std_filesystem_path);
} // namespace
