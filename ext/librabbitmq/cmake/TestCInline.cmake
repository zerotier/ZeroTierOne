#Inspired from http://www.cmake.org/Wiki/CMakeTestInline

IF(NOT DEFINED C_INLINE_KEYWORD)

  SET(INLINE_TEST_SRC "/* Inspired by autoconf's c.m4 */
static inline int static_foo() {return 0\;}
int main(int argc, char *argv[]){return 0\;}
")

  FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/CMakeTestCInline.c ${INLINE_TEST_SRC})

  FOREACH(KEYWORD "inline" "__inline__" "__inline")
    IF(NOT DEFINED C_INLINE)
      TRY_COMPILE(C_HAS_${KEYWORD}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeTestCInline.c
        COMPILE_DEFINITIONS "-Dinline=${KEYWORD}"
        )
      IF(C_HAS_${KEYWORD})
        SET(C_INLINE ${KEYWORD})
      ENDIF(C_HAS_${KEYWORD})
    ENDIF(NOT DEFINED C_INLINE)
  ENDFOREACH(KEYWORD)

  SET(C_INLINE_KEYWORD ${C_INLINE} CACHE INTERNAL "The keyword needed by the C compiler to inline a function" FORCE)
  message(STATUS "Found C inline keyword: ${C_INLINE_KEYWORD}")

ENDIF(NOT DEFINED C_INLINE_KEYWORD)
