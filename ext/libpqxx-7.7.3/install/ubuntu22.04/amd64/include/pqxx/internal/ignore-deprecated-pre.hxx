/** Start a block of deprecated code which may call other deprecated code.
 *
 * Most compilers will emit warnings when deprecated code is invoked from
 * non-deprecated code.  But some compilers (notably gcc) will always emit the
 * warning even when the calling code is also deprecated.
 *
 * This header starts a block where those warnings are suppressed.  It can be
 * included inside a code block.
 *
 * Always match the #include with a closing #include of
 * "ignore-deprecated-post.hxx".  To avoid mistakes, keep the enclosed area as
 * small as possible.
 */
#if defined(PQXX_IGNORING_DEPRECATED)
#  error "Started an 'ignore-deprecated' block inside another."
#endif

#define PQXX_IGNORING_DEPRECATED

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif // __GNUC__

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4996)
#endif
