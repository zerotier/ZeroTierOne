/** Implementation of the pqxx::stream_to class.
 *
 * pqxx::stream_to enables optimized batch updates to a database table.
 *
 * Copyright (c) 2000-2022, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this
 * mistake, or contact the author.
 */
#include "pqxx-source.hxx"

#include "pqxx/internal/header-pre.hxx"

#include "pqxx/internal/concat.hxx"
#include "pqxx/internal/gates/connection-stream_to.hxx"
#include "pqxx/stream_from.hxx"
#include "pqxx/stream_to.hxx"

#include "pqxx/internal/header-post.hxx"


namespace
{
using namespace std::literals;

void begin_copy(
  pqxx::transaction_base &tx, std::string_view table, std::string_view columns)
{
  tx.exec0(
    std::empty(columns) ?
      pqxx::internal::concat("COPY "sv, table, " FROM STDIN"sv) :
      pqxx::internal::concat(
        "COPY "sv, table, "("sv, columns, ") FROM STDIN"sv));
}
} // namespace


pqxx::stream_to::~stream_to() noexcept
{
  try
  {
    complete();
  }
  catch (std::exception const &e)
  {
    reg_pending_error(e.what());
  }
}


void pqxx::stream_to::write_raw_line(std::string_view text)
{
  internal::gate::connection_stream_to{m_trans.conn()}.write_copy_line(text);
}


void pqxx::stream_to::write_buffer()
{
  if (not std::empty(m_buffer))
  {
    // In append_to_buffer() we write a tab after each field.  We only want a
    // tab _between_ fields.  Remove that last one.
    assert(m_buffer[std::size(m_buffer) - 1] == '\t');
    m_buffer.resize(std::size(m_buffer) - 1);
  }
  write_raw_line(m_buffer);
  m_buffer.clear();
}


pqxx::stream_to &pqxx::stream_to::operator<<(stream_from &tr)
{
  while (tr)
  {
    const auto [line, size] = tr.get_raw_line();
    if (line.get() == nullptr)
      break;
    write_raw_line(std::string_view{line.get(), size});
  }
  return *this;
}


pqxx::stream_to::stream_to(
  transaction_base &tx, std::string_view path, std::string_view columns) :
        transaction_focus{tx, s_classname, path},
        m_scanner{get_glyph_scanner(
          pqxx::internal::enc_group(tx.conn().encoding_id()))}
{
  begin_copy(tx, path, columns);
  register_me();
}


void pqxx::stream_to::complete()
{
  if (!m_finished)
  {
    m_finished = true;
    unregister_me();
    internal::gate::connection_stream_to{m_trans.conn()}.end_copy_write();
  }
}


/// Return escape letter for c's backslash sequence, or 0 if not needed.
/** The API is a bit weird: you pass the width of the character, and its first
 * byte.  That's because we never need to escape a multibyte character anyway.
 */
constexpr char escape(std::size_t width, char c)
{
  if (width == 1u)
    switch (c)
    {
    case '\b': return 'b';
    case '\f': return 'f';
    case '\n': return 'n';
    case '\r': return 'r';
    case '\t': return 't';
    case '\v': return 'v';
    case '\\': return '\\';
    }

  PQXX_LIKELY
  return '\0';
}


void pqxx::stream_to::escape_field_to_buffer(std::string_view data)
{
  if (not std::empty(data))
  {
    // Mark the beginning of a stretch that we can copy into our buffer in one
    // go.  It feels like a waste to invoke generic multi-byte copies for every
    // individual character in this loop, most of them actually probably only
    // one byte long.
    std::size_t begin_stretch{0};

    std::size_t begin_char{0}, end;
    // XXX: find_char<'\b', '\f', '\n', '\r', '\t', \v', '\\'>().
    for (end = m_scanner(std::data(data), std::size(data), begin_char);
         begin_char < std::size(data); begin_char = end,
        end = m_scanner(std::data(data), std::size(data), begin_char))
    {
      // Escape sequence letter, if needed.
      char const esc{escape(end - begin_char, data[begin_char])};
      if (esc != '\0')
      {
        // This character needs escaping.  So, it ends any trivially copyable
        // stretch that we may have been having.

        // Copy the stretch we've built up into our buffer.
        m_buffer.append(
          std::data(data) + begin_stretch, begin_char - begin_stretch);

        // Escape the current character.
        m_buffer.push_back('\\');
        m_buffer.push_back(esc);

        // Start a new stretch, right after the current character.
        begin_stretch = end;
      }
    }
    // Copy the final stretch.
    m_buffer.append(
      std::data(data) + begin_stretch, begin_char - begin_stretch);
  }
  m_buffer.push_back('\t');
}
