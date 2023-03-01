#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Constants shared by multiple CSS Box Alignment properties
 *
 * These constants match Gecko's `NS_STYLE_ALIGN_*` constants.
 */
typedef struct {
  uint8_t bits;
} StyleAlignFlags;
/**
 * 'auto'
 */
#define StyleAlignFlags_AUTO (StyleAlignFlags){ .bits = (uint8_t)0 }
/**
 * 'normal'
 */
#define StyleAlignFlags_NORMAL (StyleAlignFlags){ .bits = (uint8_t)1 }
/**
 * 'start'
 */
#define StyleAlignFlags_START (StyleAlignFlags){ .bits = (uint8_t)(1 << 1) }
/**
 * 'end'
 */
#define StyleAlignFlags_END (StyleAlignFlags){ .bits = (uint8_t)(1 << 2) }
/**
 * 'flex-start'
 */
#define StyleAlignFlags_FLEX_START (StyleAlignFlags){ .bits = (uint8_t)(1 << 3) }

void root(StyleAlignFlags flags);
