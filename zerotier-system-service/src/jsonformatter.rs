/* This is a forked and hacked version of PrettyFormatter from:
 *
 * https://github.com/serde-rs/json/blob/master/src/ser.rs
 *
 * It is therefore under the same Apache license.
 */

use serde_json::ser::Formatter;

#[derive(Clone, Debug)]
pub struct JsonFormatter<'a> {
    current_indent: usize,
    has_value: bool,
    indent: &'a [u8],
}

fn indent<W>(wr: &mut W, n: usize, s: &[u8]) -> std::io::Result<()>
where
    W: ?Sized + std::io::Write,
{
    for _ in 0..n {
        wr.write_all(s)?;
    }
    Ok(())
}

impl<'a> JsonFormatter<'a> {
    pub fn new() -> Self {
        JsonFormatter::with_indent(b"  ")
    }

    pub fn with_indent(indent: &'a [u8]) -> Self {
        JsonFormatter { current_indent: 0, has_value: false, indent }
    }
}

impl<'a> Default for JsonFormatter<'a> {
    fn default() -> Self {
        JsonFormatter::new()
    }
}

impl<'a> Formatter for JsonFormatter<'a> {
    fn begin_array<W>(&mut self, writer: &mut W) -> std::io::Result<()>
    where
        W: ?Sized + std::io::Write,
    {
        self.current_indent += 1;
        self.has_value = false;
        writer.write_all(b"[")
    }

    fn end_array<W>(&mut self, writer: &mut W) -> std::io::Result<()>
    where
        W: ?Sized + std::io::Write,
    {
        self.current_indent -= 1;
        if self.has_value {
            writer.write_all(b" ]")
        } else {
            writer.write_all(b"]")
        }
    }

    fn begin_array_value<W>(&mut self, writer: &mut W, first: bool) -> std::io::Result<()>
    where
        W: ?Sized + std::io::Write,
    {
        if first {
            writer.write_all(b" ")?;
        } else {
            writer.write_all(b", ")?;
        }
        Ok(())
    }

    fn end_array_value<W>(&mut self, _writer: &mut W) -> std::io::Result<()>
    where
        W: ?Sized + std::io::Write,
    {
        self.has_value = true;
        Ok(())
    }

    fn begin_object<W>(&mut self, writer: &mut W) -> std::io::Result<()>
    where
        W: ?Sized + std::io::Write,
    {
        self.current_indent += 1;
        self.has_value = false;
        writer.write_all(b"{")
    }

    fn end_object<W>(&mut self, writer: &mut W) -> std::io::Result<()>
    where
        W: ?Sized + std::io::Write,
    {
        self.current_indent -= 1;

        if self.has_value {
            writer.write_all(b"\n")?;
            indent(writer, self.current_indent, self.indent)?;
        }

        writer.write_all(b"}")
    }

    fn begin_object_key<W>(&mut self, writer: &mut W, first: bool) -> std::io::Result<()>
    where
        W: ?Sized + std::io::Write,
    {
        if first {
            writer.write_all(b"\n")?;
        } else {
            writer.write_all(b",\n")?;
        }
        indent(writer, self.current_indent, self.indent)
    }

    fn begin_object_value<W>(&mut self, writer: &mut W) -> std::io::Result<()>
    where
        W: ?Sized + std::io::Write,
    {
        writer.write_all(b": ")
    }

    fn end_object_value<W>(&mut self, _writer: &mut W) -> std::io::Result<()>
    where
        W: ?Sized + std::io::Write,
    {
        self.has_value = true;
        Ok(())
    }
}
