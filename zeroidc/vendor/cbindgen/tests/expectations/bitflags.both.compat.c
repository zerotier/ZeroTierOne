#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Constants shared by multiple CSS Box Alignment properties
 *
 * These constants match Gecko's `NS_STYLE_ALIGN_*` constants.
 */
typedef struct AlignFlags {
  uint8_t bits;
} AlignFlags;
/**
 * 'auto'
 */
#define AlignFlags_AUTO (AlignFlags){ .bits = (uint8_t)0 }
/**
 * 'normal'
 */
#define AlignFlags_NORMAL (AlignFlags){ .bits = (uint8_t)1 }
/**
 * 'start'
 */
#define AlignFlags_START (AlignFlags){ .bits = (uint8_t)(1 << 1) }
/**
 * 'end'
 */
#define AlignFlags_END (AlignFlags){ .bits = (uint8_t)(1 << 2) }
/**
 * 'flex-start'
 */
#define AlignFlags_FLEX_START (AlignFlags){ .bits = (uint8_t)(1 << 3) }

typedef struct DebugFlags {
  uint32_t bits;
} DebugFlags;
/**
 * Flag with the topmost bit set of the u32
 */
#define DebugFlags_BIGGEST_ALLOWED (DebugFlags){ .bits = (uint32_t)(1 << 31) }

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void root(struct AlignFlags flags, struct DebugFlags bigger_flags);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
