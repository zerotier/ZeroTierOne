/// End a code block started by "ignore-deprecated-pre.hxx".

#if !defined(PQXX_IGNORING_DEPRECATED)
#  error "Ended an 'ignore-deprecated' block while none was active."
#endif

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif // __GNUC__

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#undef PQXX_IGNORING_DEPRECATED
