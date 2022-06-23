# ##############################################################################
# AUTOMATICALLY GENERATED FILE -- DO NOT EDIT.
#
# This file is generated automatically by libpqxx's template2mak.py script, and
# will be rewritten from time to time.
#
# If you modify this file, chances are your modifications will be lost.
#
# The template2mak.py script should be available in the tools directory of the
# libpqxx source archive.
#
# Generated from template './include/CMakeLists.txt.template'.
# ##############################################################################
install(
    DIRECTORY pqxx "${PROJECT_BINARY_DIR}/include/pqxx"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    # For each X.hxx, install both X.hxx itself and plain X.
    PATTERN *.hxx
    # TODO: Is there any way to do this with CMake's globbing?
    PATTERN array
    PATTERN binarystring
    PATTERN blob
    PATTERN composite
    PATTERN connection
    PATTERN cursor
    PATTERN dbtransaction
    PATTERN errorhandler
    PATTERN except
    PATTERN field
    PATTERN isolation
    PATTERN largeobject
    PATTERN nontransaction
    PATTERN notification
    PATTERN params
    PATTERN pipeline
    PATTERN prepared_statement
    PATTERN range
    PATTERN result
    PATTERN robusttransaction
    PATTERN row
    PATTERN separated_list
    PATTERN strconv
    PATTERN stream_from
    PATTERN stream_to
    PATTERN subtransaction
    PATTERN time
    PATTERN transaction
    PATTERN transaction_base
    PATTERN transaction_focus
    PATTERN transactor
    PATTERN types
    PATTERN util
    PATTERN version
    PATTERN zview
    PATTERN internal/*.hxx
    PATTERN internal/gates/*.hxx
    PATTERN config-public-compiler.h
    PATTERN pqxx
    PATTERN doc EXCLUDE
)

install(
    DIRECTORY pqxx/doc/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    FILES_MATCHING
    PATTERN *.md
)
