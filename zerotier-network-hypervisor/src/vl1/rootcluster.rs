/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::BTreeSet;
use std::io::Write;

use crate::util::buffer::Buffer;
use crate::util::marshalable::Marshalable;
use crate::vl1::identity::*;
use crate::vl1::protocol::PACKET_SIZE_MAX;
use crate::vl1::Endpoint;

/// Description of a member of a root cluster.
#[derive(Clone, PartialEq, Eq)]
pub struct Root {
    /// Full identity of this node.
    pub identity: Identity,

    /// Endpoints for this root or None if this is a former member attesting to an update that removes it.
    pub endpoints: Option<BTreeSet<Endpoint>>,

    /// Signature of entire cluster by this identity.
    pub cluster_signature: Vec<u8>,

    /// Flags field (currently unused).
    pub flags: u64,
}

impl PartialOrd for Root {
    #[inline(always)]
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        self.identity.partial_cmp(&other.identity)
    }
}

impl Ord for Root {
    #[inline(always)]
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.identity.cmp(&other.identity)
    }
}

/// Signed description of a cluster of root nodes.
///
/// Root cluster definitions must be signed by all current participating nodes' identities. In addition
/// there is an update authorization model based on authorization by at least N-1 previous members.
/// See the documentation of should_replace().
///
/// To build a cluster definition first use new(), then use add() to add all members, then have each member
/// use sign() to sign its entry. All members must sign after all calls to add() have been made since everyone
/// must sign the same definition.
#[derive(Clone, PartialEq, Eq)]
pub struct RootCluster {
    /// An arbitrary name, which could be something like a domain.
    pub name: String,

    /// A monotonically increasing revision number (doesn't have to be sequential).
    pub revision: u64,

    /// A set of Root nodes that are current or immediately former members of this cluster.
    /// This will always be sorted by member identity.
    pub members: Vec<Root>,
}

impl RootCluster {
    pub fn new(name: String, revision: u64) -> Self {
        Self { name, revision, members: Vec::new() }
    }

    fn marshal_internal<const BL: usize>(&self, buf: &mut Buffer<BL>, include_signatures: bool) -> std::io::Result<()> {
        buf.append_u8(0)?; // version byte for future use
        buf.append_varint(self.name.as_bytes().len() as u64)?;
        buf.append_bytes(self.name.as_bytes())?;
        buf.append_varint(self.revision)?;
        buf.append_varint(self.members.len() as u64)?;
        for m in self.members.iter() {
            m.identity.marshal_with_options(buf, IDENTITY_ALGORITHM_ALL, false)?;
            if m.endpoints.is_some() {
                let endpoints = m.endpoints.as_ref().unwrap();
                buf.append_varint(endpoints.len() as u64)?;
                for a in endpoints.iter() {
                    a.marshal(buf)?;
                }
            } else {
                buf.append_varint(0)?;
            }
            if include_signatures {
                buf.append_varint(m.cluster_signature.len() as u64)?;
                buf.append_bytes(m.cluster_signature.as_slice())?;
            }
            buf.append_varint(m.flags)?;
            buf.append_varint(0)?; // size of additional fields for future use
        }
        buf.append_varint(0)?; // size of additional fields for future use
        Ok(())
    }

    /// Internal method to marshal without signatures for use during sign and verify.
    fn marshal_for_signing(&self) -> Buffer<MAX_MARSHAL_SIZE> {
        let mut tmp = Buffer::<MAX_MARSHAL_SIZE>::new();
        assert!(self.marshal_internal(&mut tmp, false).is_ok());
        tmp
    }

    /// Verify signatures present in this root cluster definition.
    pub fn verify(&self) -> bool {
        if self.members.is_empty() {
            return false;
        }

        let tmp = self.marshal_for_signing();
        for m in self.members.iter() {
            if m.cluster_signature.is_empty() || !m.identity.verify(tmp.as_bytes(), m.cluster_signature.as_slice()) {
                return false;
            }
        }

        return true;
    }

    /// Add a member to this definition, replacing any current entry for this identity.
    pub fn add<'a, I: Iterator<Item = &'a Endpoint>>(&mut self, member_identity: &Identity, endpoints: Option<I>) {
        self.members.retain(|m| !m.identity.eq(member_identity));
        let _ = self.members.push(Root {
            identity: member_identity.clone_without_secret(),
            endpoints: endpoints.map(|endpoints| {
                let mut tmp = BTreeSet::new();
                for a in endpoints {
                    tmp.insert(a.clone());
                }
                tmp
            }),
            cluster_signature: Vec::new(),
            flags: 0,
        });
        self.members.sort();
    }

    /// Sign this definition, returning true on success.
    ///
    /// A return value of false indicates that this member wasn't in the definition or this identity
    /// did not have its secret to sign.
    ///
    /// All current members must sign whether they are disabled (witnessing) or active. The verify()
    /// method will return true when signing is complete.
    pub fn sign(&mut self, member_identity: &Identity) -> bool {
        let signature = member_identity.sign(self.marshal_for_signing().as_bytes(), IDENTITY_ALGORITHM_ALL, false);
        let unsigned_entry = self.members.iter().find_map(|m| if m.identity.eq(member_identity) { Some(m.clone()) } else { None });
        if unsigned_entry.is_some() && signature.is_some() {
            let unsigned_entry = unsigned_entry.unwrap();
            self.members.retain(|m| !m.identity.eq(member_identity));
            let _ = self.members.push(Root {
                identity: unsigned_entry.identity,
                endpoints: unsigned_entry.endpoints,
                cluster_signature: signature.unwrap(),
                flags: unsigned_entry.flags,
            });
            self.members.sort();
            return true;
        }
        return false;
    }

    /// Check whether this root cluster definition should replace a previous one.
    ///
    /// A root cluster definition replaces an older version of (1) the name is equal, (2)
    /// the revision is higher, and (3) the new definition was signed by at least N-1 active
    /// (not disabled) members of the previous definition. So for example a cluster of four
    /// root nodes can replace one member if three cluster members sign the update, but to
    /// remove two at a time one of the exiting members would have to sign. This is done by
    /// adding it with None as its address list, making it disabled. Disabled members function
    /// only as signers (witnesses).
    ///
    /// There is one edge case though. If a cluster definition has only one member, that one
    /// member must sign the next update. N-1 is not permitted to be less than one. If that was
    /// not the case it would be possible for anyone to update a one-member definition!
    ///
    /// This DOES call verify() on itself prior to checking to avoid the disastrous error
    /// of forgetting to verify signatures on a new definition.
    ///
    /// Be sure the semantics are right and this method is being called with 'self' being the
    /// new root cluster definition and 'previous' being the current/old one.
    pub fn should_replace(&self, previous: &Self) -> bool {
        if self.name.eq(&previous.name) && self.revision > previous.revision && self.verify() {
            let mut my_signers = BTreeSet::new();
            for m in self.members.iter() {
                my_signers.insert(m.identity.fingerprint.clone());
            }

            let mut previous_count: isize = 0;
            let mut witness_count: isize = 0;
            for m in previous.members.iter() {
                if m.endpoints.is_some() {
                    previous_count += 1;
                    witness_count += my_signers.contains(&m.identity.fingerprint) as isize;
                }
            }

            witness_count >= (previous_count - 1).max(1)
        } else {
            false
        }
    }
}

impl Marshalable for RootCluster {
    const MAX_MARSHAL_SIZE: usize = PACKET_SIZE_MAX;

    #[inline(always)]
    fn marshal<const BL: usize>(&self, buf: &mut Buffer<BL>) -> std::io::Result<()> {
        self.marshal_internal(buf, true)
    }

    fn unmarshal<const BL: usize>(buf: &Buffer<BL>, cursor: &mut usize) -> std::io::Result<Self> {
        let mut rc = Self::new(String::new(), 0);
        if buf.read_u8(cursor)? != 0 {
            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "unsupported version"));
        }

        let name_len = buf.read_varint(cursor)?;
        rc.name = String::from_utf8(buf.read_bytes(name_len as usize, cursor)?.to_vec()).map_err(|_| std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid UTF8"))?;

        rc.revision = buf.read_varint(cursor)?;

        let member_count = buf.read_varint(cursor)?;
        for _ in 0..member_count {
            let mut m = Root {
                identity: Identity::unmarshal(buf, cursor)?,
                endpoints: None,
                cluster_signature: Vec::new(),
                flags: 0,
            };

            let endpoint_count = buf.read_varint(cursor)?;
            if endpoint_count > 0 {
                let mut endpoints = BTreeSet::new();
                for _ in 0..endpoint_count {
                    endpoints.insert(Endpoint::unmarshal(buf, cursor)?);
                }
                let _ = m.endpoints.insert(endpoints);
            }

            let signature_size = buf.read_varint(cursor)?;
            let _ = m.cluster_signature.write_all(buf.read_bytes(signature_size as usize, cursor)?);

            m.flags = buf.read_varint(cursor)?;

            *cursor += buf.read_varint(cursor)? as usize;

            rc.members.push(m);
        }

        *cursor += buf.read_varint(cursor)? as usize;
        if *cursor > buf.len() {
            return std::io::Result::Err(std::io::Error::new(std::io::ErrorKind::InvalidData, "invalid length"));
        }

        rc.members.sort();

        return Ok(rc);
    }
}
