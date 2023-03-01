//! Context types for polling systems, e.g. kqueue and epoll.

#![allow(unsafe_code)]

use crate::fd::{AsFd, AsRawFd, BorrowedFd, FromRawFd, IntoRawFd, OwnedFd, RawFd};

use core::fmt;
use core::marker::PhantomData;
use core::ops::Deref;

/// A reference to a `T`.
pub struct Ref<'a, T> {
    t: T,
    _phantom: PhantomData<&'a T>,
}

impl<'a, T> Ref<'a, T> {
    #[inline]
    fn new(t: T) -> Self {
        Self {
            t,
            _phantom: PhantomData,
        }
    }

    #[inline]
    fn consume(self) -> T {
        self.t
    }
}

impl<'a, T> Deref for Ref<'a, T> {
    type Target = T;

    #[inline]
    fn deref(&self) -> &T {
        &self.t
    }
}

impl<'a, T: fmt::Debug> fmt::Debug for Ref<'a, T> {
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.t.fmt(fmt)
    }
}

/// A trait for data stored within an [`Epoll`] instance.
///
/// [`Epoll`]: crate::io::epoll::Epoll
pub trait Context {
    /// The type of an element owned by this context.
    type Data;

    /// The type of a value used to refer to an element owned by this context.
    type Target: AsFd;

    /// Assume ownership of `data`, and returning a `Target`.
    fn acquire<'call>(&self, data: Self::Data) -> Ref<'call, Self::Target>;

    /// Encode `target` as a `u64`. The only requirement on this value is that
    /// it be decodable by `decode`.
    fn encode(&self, target: Ref<'_, Self::Target>) -> u64;

    /// Decode `raw`, which is a value encoded by `encode`, into a `Target`.
    ///
    /// # Safety
    ///
    /// `raw` must be a `u64` value returned from `encode`, from the same
    /// context, and within the context's lifetime.
    unsafe fn decode<'call>(&self, raw: u64) -> Ref<'call, Self::Target>;

    /// Release ownership of the value referred to by `target` and return it.
    fn release(&self, target: Ref<'_, Self::Target>) -> Self::Data;
}

/// A type implementing [`Context`] where the `Data` type is `BorrowedFd<'a>`.
pub struct Borrowing<'a> {
    _phantom: PhantomData<BorrowedFd<'a>>,
}

impl<'a> Context for Borrowing<'a> {
    type Data = BorrowedFd<'a>;
    type Target = BorrowedFd<'a>;

    #[inline]
    fn acquire<'call>(&self, data: Self::Data) -> Ref<'call, Self::Target> {
        Ref::new(data)
    }

    #[inline]
    fn encode(&self, target: Ref<'_, Self::Target>) -> u64 {
        target.as_raw_fd() as u64
    }

    #[inline]
    unsafe fn decode<'call>(&self, raw: u64) -> Ref<'call, Self::Target> {
        Ref::new(BorrowedFd::<'a>::borrow_raw(raw as RawFd))
    }

    #[inline]
    fn release(&self, target: Ref<'_, Self::Target>) -> Self::Data {
        target.consume()
    }
}

/// A type implementing [`Context`] where the `Data` type is `T`, a type
/// implementing `From<OwnedFd>` and `From<T> for OwnedFd`.
///
/// This may be used with [`OwnedFd`], or higher-level types like
/// [`std::fs::File`] or [`std::net::TcpStream`].
#[cfg(not(feature = "rustc-dep-of-std"))]
pub struct Owning<'context, T: Into<OwnedFd> + From<OwnedFd>> {
    _phantom: PhantomData<&'context T>,
}

#[cfg(not(feature = "rustc-dep-of-std"))]
impl<'context, T: Into<OwnedFd> + From<OwnedFd>> Owning<'context, T> {
    /// Creates a new empty `Owning`.
    #[allow(clippy::new_without_default)] // This is a specialized type that doesn't need to be generically constructible.
    #[inline]
    pub fn new() -> Self {
        Self {
            _phantom: PhantomData,
        }
    }
}

#[cfg(not(feature = "rustc-dep-of-std"))]
impl<'context, T: AsFd + Into<OwnedFd> + From<OwnedFd>> Context for Owning<'context, T> {
    type Data = T;
    type Target = BorrowedFd<'context>;

    #[inline]
    fn acquire<'call>(&self, data: Self::Data) -> Ref<'call, Self::Target> {
        let fd: OwnedFd = data.into();
        let raw_fd = fd.into_raw_fd();
        // Safety: `epoll` will assign ownership of the file descriptor to the
        // kernel epoll object. We use `Into<OwnedFd>`+`IntoRawFd` to consume
        // the `Data` and extract the raw file descriptor and then "borrow" it
        // with `borrow_raw` knowing that the borrow won't outlive the
        // kernel epoll object.
        unsafe { Ref::new(BorrowedFd::<'context>::borrow_raw(raw_fd)) }
    }

    #[inline]
    fn encode(&self, target: Ref<'_, Self::Target>) -> u64 {
        target.as_fd().as_raw_fd() as u64
    }

    #[inline]
    unsafe fn decode<'call>(&self, raw: u64) -> Ref<'call, Self::Target> {
        Ref::new(BorrowedFd::<'context>::borrow_raw(raw as RawFd))
    }

    #[inline]
    fn release(&self, target: Ref<'_, Self::Target>) -> Self::Data {
        // The file descriptor was held by the kernel epoll object and is now
        // being released, so we can create a new `OwnedFd` that assumes
        // ownership.
        let raw_fd = target.consume().as_raw_fd();
        unsafe { T::from(OwnedFd::from_raw_fd(raw_fd).into()) }
    }
}
