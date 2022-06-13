from libc.stdint cimport int8_t, int16_t, int32_t, int64_t, intptr_t
from libc.stdint cimport uint8_t, uint16_t, uint32_t, uint64_t, uintptr_t
cdef extern from *:
  ctypedef bint bool
  ctypedef struct va_list

cdef extern from *:

  # Constants shared by multiple CSS Box Alignment properties
  #
  # These constants match Gecko's `NS_STYLE_ALIGN_*` constants.
  cdef struct AlignFlags:
    uint8_t bits;
  # 'auto'
  const AlignFlags AlignFlags_AUTO # = <AlignFlags>{ <uint8_t>0 }
  # 'normal'
  const AlignFlags AlignFlags_NORMAL # = <AlignFlags>{ <uint8_t>1 }
  # 'start'
  const AlignFlags AlignFlags_START # = <AlignFlags>{ <uint8_t>(1 << 1) }
  # 'end'
  const AlignFlags AlignFlags_END # = <AlignFlags>{ <uint8_t>(1 << 2) }
  # 'flex-start'
  const AlignFlags AlignFlags_FLEX_START # = <AlignFlags>{ <uint8_t>(1 << 3) }

  cdef struct DebugFlags:
    uint32_t bits;
  # Flag with the topmost bit set of the u32
  const DebugFlags DebugFlags_BIGGEST_ALLOWED # = <DebugFlags>{ <uint32_t>(1 << 31) }

  void root(AlignFlags flags, DebugFlags bigger_flags);
