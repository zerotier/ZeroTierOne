use fastcdc::v2020::{FastCDC, MINIMUM_MIN};
use std::io::Write;
use zerotier_crypto::hash::{SHA384, SHA384_HASH_SIZE};
use zerotier_utils::error::{InvalidFormatError, InvalidParameterError};

const MAX_RECURSION_DEPTH: usize = 64;

/// Recursively scatter/gather chunked object assembler.
pub struct ObjectAssembler {
    data_chunks: Vec<Vec<u8>>,
    need: Vec<u8>,
}

impl ObjectAssembler {
    /// Create a new assembler to gather an object given its root hash list.
    pub fn init(hash_list: Vec<u8>) -> Self {
        Self { data_chunks: Vec::new(), need: hash_list }
    }

    fn gather_recursive<GetChunk: FnMut(&[u8]) -> Option<Vec<u8>>>(
        hl: &[u8],
        new_hl: &mut Vec<u8>,
        get_chunk: &mut GetChunk,
        have_all_data_chunk_hashes: &mut bool,
        depth: usize,
    ) -> Result<(), InvalidFormatError> {
        if (hl.len() % SHA384_HASH_SIZE) != 0 {
            return Err(InvalidFormatError);
        }
        for h in hl.chunks_exact(SHA384_HASH_SIZE) {
            if (h[SHA384_HASH_SIZE - 1] & 0x01) != 0 {
                if let Some(chunk) = get_chunk(h) {
                    if depth < MAX_RECURSION_DEPTH {
                        Self::gather_recursive(chunk.as_slice(), new_hl, get_chunk, have_all_data_chunk_hashes, depth + 1)?;
                        continue;
                    } else {
                        return Err(InvalidFormatError);
                    }
                }
                *have_all_data_chunk_hashes = false;
            }
            let _ = new_hl.write_all(h);
        }
        return Ok(());
    }

    /// Try to assemble this object, using the supplied function to request chunks we don't have.
    ///
    /// Once all chunks are retrieved this will return Ok(Some(object)). An error return can occur if a chunk
    /// is invalid or the maximum recursion depth is reached.
    pub fn gather<GetChunk: FnMut(&[u8]) -> Option<Vec<u8>>>(&mut self, mut get_chunk: GetChunk) -> Result<Option<Vec<u8>>, InvalidFormatError> {
        let mut new_need = Vec::with_capacity(self.need.len());
        let mut have_all_data_chunk_hashes = true;
        Self::gather_recursive(self.need.as_slice(), &mut new_need, &mut get_chunk, &mut have_all_data_chunk_hashes, 0)?;
        std::mem::swap(&mut self.need, &mut new_need);

        if have_all_data_chunk_hashes {
            self.data_chunks.resize(self.need.len() / SHA384_HASH_SIZE, Vec::new());

            new_need.clear();
            let mut cn = 0;
            for h in self.need.chunks_exact(SHA384_HASH_SIZE) {
                if let Some(chunk) = get_chunk(h) {
                    *self.data_chunks.get_mut(cn).unwrap() = chunk;
                } else {
                    let _ = new_need.write_all(h);
                }
                cn += 1;
            }
            self.need = new_need;

            if self.need.is_empty() {
                let mut obj_size = 0;
                for dc in self.data_chunks.iter() {
                    obj_size += dc.len();
                }
                let mut obj = Vec::with_capacity(obj_size);
                for dc in self.data_chunks.iter() {
                    let _ = obj.write_all(dc.as_slice());
                }
                return Ok(Some(obj));
            }
        }

        return Ok(None);
    }
}

/// Decompose an object into a series of chunks identified by SHA384 hashes.
///
/// This splits the supplied binary object into chunks using the FastCDC2020 content defined chunking
/// algorithm. For each chunk a SHA384 hash is computed and added to a hash list. If the resulting
/// hash list is larger than max_chunk_size it is further chunked in a simple deterministic way to
/// yield hashes that point to further lists of hashes. The least significant bit in each hash is
/// set to 0 if the hash points to a chunk of data or 1 if it points to a chunk of hashes.
///
/// The supplied function is called to output each chunk except for the root hash list, which is
/// returned.
///
/// * `obj` - Blob to decompose
/// * `max_chunk_size` - Maximum size of any chunk including root hash list (minimum allowed: 256)
/// * `store_chunk` - Function that is called to store each chunk other than the root hash list
pub fn scatter<F: FnMut([u8; SHA384_HASH_SIZE], &[u8])>(
    obj: &[u8],
    max_chunk_size: u32,
    mut store_chunk: F,
) -> Result<Vec<u8>, InvalidParameterError> {
    if max_chunk_size < 512 {
        return Err(InvalidParameterError("max chunk size must be >= 512"));
    }
    let mut root_hash_list = Vec::with_capacity(max_chunk_size as usize);

    for chunk in FastCDC::new(obj, (max_chunk_size / 4).max(MINIMUM_MIN), max_chunk_size / 2, max_chunk_size) {
        let chunk = &obj[chunk.offset..chunk.offset + chunk.length];
        let mut chunk_hash = SHA384::hash(chunk);
        chunk_hash[SHA384_HASH_SIZE - 1] &= 0xfe; // chunk of data
        let _ = root_hash_list.write_all(&chunk_hash);
        store_chunk(chunk_hash, chunk);
    }

    if root_hash_list.len() > (max_chunk_size as usize) {
        let max_hashes_per_chunk = ((max_chunk_size / (SHA384_HASH_SIZE as u32)) * (SHA384_HASH_SIZE as u32)) as usize;
        let mut new_root_hash_list = Vec::with_capacity(max_chunk_size as usize);
        let mut recursion_depth = 0;
        loop {
            let mut r = root_hash_list.as_slice();
            while !r.is_empty() {
                let clen = r.len().min(max_hashes_per_chunk);
                let chunk = &r[..clen];
                if clen > SHA384_HASH_SIZE && (new_root_hash_list.len() + clen) > (max_chunk_size as usize) {
                    let mut chunk_hash = SHA384::hash(chunk);
                    chunk_hash[SHA384_HASH_SIZE - 1] |= 0x01; // chunk of hashes
                    let _ = new_root_hash_list.write_all(&chunk_hash);
                    store_chunk(chunk_hash, chunk);
                    r = &r[clen..];
                } else {
                    let _ = new_root_hash_list.write_all(chunk);
                    break;
                }
            }
            std::mem::swap(&mut root_hash_list, &mut new_root_hash_list);

            if root_hash_list.len() <= (max_chunk_size as usize) {
                break;
            } else {
                new_root_hash_list.clear();
                if recursion_depth >= MAX_RECURSION_DEPTH {
                    return Err(InvalidParameterError("max recursion depth exceeded"));
                }
                recursion_depth += 1;
            }
        }
    }

    return Ok(root_hash_list);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;
    use zerotier_utils::hex;

    #[test]
    fn rcdcb_random_blobs() {
        let mut random_data = Vec::new();
        random_data.resize(1024 * 1024 * 32, 0);
        zerotier_crypto::random::fill_bytes_secure(random_data.as_mut());

        let mut chunks = HashMap::new();
        for _ in 0..1024 {
            chunks.clear();
            let test_blob = ((zerotier_crypto::random::xorshift64_random() as usize) % (random_data.len() - 1)) + 1;
            let test_blob = &random_data.as_slice()[..test_blob];

            let root_hash_list = scatter(test_blob, 1024, |k, v| {
                //println!("{}", hex::to_string(&k));
                chunks.insert(k, v.to_vec());
            });
            println!("{} chunks", chunks.len());
        }
    }
}
