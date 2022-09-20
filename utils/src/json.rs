use serde::de::DeserializeOwned;
use serde::Serialize;
use serde_json::ser::Formatter;

/// Recursively patch a JSON object.
///
/// This is slightly different from a usual JSON merge. For objects in the target their fields
/// are updated by recursively calling json_patch if the same field is present in the source.
/// If the source tries to set an object to something other than another object, this is ignored.
/// Other fields are replaced. This is used for RESTful config object updates. The depth limit
/// field is to prevent stack overflows via the API.
pub fn json_patch(target: &mut serde_json::value::Value, source: &serde_json::value::Value, depth_limit: usize) {
    if target.is_object() {
        if source.is_object() {
            let target = target.as_object_mut().unwrap();
            let source = source.as_object().unwrap();
            for kv in target.iter_mut() {
                let _ = source.get(kv.0).map(|new_value| {
                    if depth_limit > 0 {
                        json_patch(kv.1, new_value, depth_limit - 1)
                    }
                });
            }
            for kv in source.iter() {
                if !target.contains_key(kv.0) && !kv.1.is_null() {
                    target.insert(kv.0.clone(), kv.1.clone());
                }
            }
        }
    } else if *target != *source {
        *target = source.clone();
    }
}

/// Patch a serializable object with the fields present in a JSON object.
///
/// If there are no changes, None is returned. The depth limit is passed through to json_patch and
/// should be set to a sanity check value to prevent overflows.
pub fn json_patch_object<O: Serialize + DeserializeOwned + Eq>(
    obj: O,
    patch: &str,
    depth_limit: usize,
) -> Result<Option<O>, serde_json::Error> {
    serde_json::from_str::<serde_json::value::Value>(patch).map_or_else(
        |e| Err(e),
        |patch| {
            serde_json::value::to_value(&obj).map_or_else(
                |e| Err(e),
                |mut obj_value| {
                    json_patch(&mut obj_value, &patch, depth_limit);
                    serde_json::value::from_value::<O>(obj_value).map_or_else(
                        |e| Err(e),
                        |obj_merged| {
                            if obj == obj_merged {
                                Ok(None)
                            } else {
                                Ok(Some(obj_merged))
                            }
                        },
                    )
                },
            )
        },
    )
}

/// Shortcut to use serde_json to serialize an object, returns "null" on error.
pub fn to_json<O: serde::Serialize>(o: &O) -> String {
    serde_json::to_string(o).unwrap_or("null".into())
}

/// Shortcut to use serde_json to serialize an object, returns "null" on error.
pub fn to_json_pretty<O: serde::Serialize>(o: &O) -> String {
    let mut buf = Vec::new();
    let mut ser = serde_json::Serializer::with_formatter(&mut buf, PrettyFormatter::new());
    if o.serialize(&mut ser).is_ok() {
        String::from_utf8(buf).unwrap_or_else(|_| "null".into())
    } else {
        "null".into()
    }
}

/// JSON formatter that looks a bit better than the Serde default.
pub struct PrettyFormatter<'a> {
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

impl<'a> PrettyFormatter<'a> {
    pub fn new() -> Self {
        Self::with_indent(b"  ")
    }

    pub fn with_indent(indent: &'a [u8]) -> Self {
        Self { current_indent: 0, has_value: false, indent }
    }
}

impl<'a> Default for PrettyFormatter<'a> {
    fn default() -> Self {
        Self::new()
    }
}

impl<'a> Formatter for PrettyFormatter<'a> {
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
