# - Convert XML docBook files to various formats
# This will convert XML docBook files to various formats like:
# man html txt dvi ps pdf
#  macro XMLTO(outfiles infiles... MODES modes...)

find_program ( XMLTO_EXECUTABLE
  NAMES xmlto
  DOC   "path to the xmlto docbook xslt frontend"  
)


include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(XMLTO
  REQUIRED_VARS XMLTO_EXECUTABLE)

mark_as_advanced( XMLTO_EXECUTABLE )

macro ( _XMLTO_FILE outfiles mode)
  #special settings
  set ( XMLTO_FILEEXT_man 1 )
  set ( XMLTO_MODE_html xhtml-nochunks )

  if ( NOT XMLTO_MODE_${mode})
    set ( XMLTO_MODE_${mode} ${mode} )
  endif ( NOT XMLTO_MODE_${mode} )
  if ( NOT XMLTO_FILEEXT_${mode} )
    set ( XMLTO_FILEEXT_${mode} ${mode} )
  endif ( NOT XMLTO_FILEEXT_${mode} )

  foreach ( dbFile ${ARGN} )
    #TODO: set XMLTO_FILEEXT_man to value from <manvolnum>
    if ( "${mode}" STREQUAL "man" )
      file ( READ "${dbFile}" _DB_FILE_CONTENTS )
      string ( REGEX MATCH "<manvolnum>[^<]*" XMLTO_FILEEXT_${mode} "${_DB_FILE_CONTENTS}" )
      string ( REGEX REPLACE "^<manvolnum>" "" XMLTO_FILEEXT_${mode} "${XMLTO_FILEEXT_${mode}}" )
      string ( REGEX REPLACE "[[:space:]]" "" XMLTO_FILEEXT_${mode} "${XMLTO_FILEEXT_${mode}}" )
    endif ( "${mode}" STREQUAL "man" )

    get_filename_component ( dbFilePath ${CMAKE_CURRENT_BINARY_DIR}/${dbFile} PATH )
    get_filename_component ( dbFileWE ${dbFile} NAME_WE )
    get_filename_component ( dbFileAbsWE ${dbFilePath}/${dbFileWE} ABSOLUTE )

    add_custom_command (
      OUTPUT            ${dbFileAbsWE}.${XMLTO_FILEEXT_${mode}}
      COMMAND           ${XMLTO_EXECUTABLE} ${XMLTO_COMMAND_ARGS} -o ${dbFilePath}
                        ${XMLTO_MODE_${mode}} "${CMAKE_CURRENT_SOURCE_DIR}/${dbFile}"
      MAIN_DEPENDENCY   ${CMAKE_CURRENT_SOURCE_DIR}/${dbFile}
      DEPENDS           ${XMLTO_DEPENDS}
      VERBATIM
    )

    set ( ${outfiles}
      ${${outfiles}}
      ${dbFileAbsWE}.${XMLTO_FILEEXT_${mode}}
    )
  endforeach ( dbFile )
endmacro ( _XMLTO_FILE outfiles )

macro ( XMLTO )
  set ( XMLTO_MODES )
  set ( XMLTO_FILES )
  set ( XMLTO_HAS_MODES false )
  set ( XMLTO_ADD_DEFAULT false )
  foreach ( arg ${ARGN} )
    if ( ${arg} STREQUAL "MODES" )
      set ( XMLTO_HAS_MODES true )
    elseif ( ${arg} STREQUAL "ALL" )
      set ( XMLTO_ADD_DEFAULT true )
    else ( ${arg} STREQUAL "MODES" )
      if ( XMLTO_HAS_MODES )
  set ( XMLTO_MODES ${XMLTO_MODES} ${arg} )
      else ( XMLTO_HAS_MODES )    
  set ( XMLTO_FILES ${XMLTO_FILES} ${arg} )
      endif ( XMLTO_HAS_MODES )
    endif ( ${arg} STREQUAL "MODES" )
  endforeach ( arg ${ARGN} )
  if ( NOT XMLTO_MODES )
    set ( XMLTO_MODES html )
  endif ( NOT XMLTO_MODES )

  foreach ( mode ${XMLTO_MODES} )
    _xmlto_file ( XMLTO_FILES_${mode} ${mode} ${XMLTO_FILES} )
    if ( XMLTO_ADD_DEFAULT )
      add_custom_target ( ${mode} ALL
  DEPENDS ${XMLTO_FILES_${mode}}
  VERBATIM
      )
    else ( XMLTO_ADD_DEFAULT )
      add_custom_target ( ${mode}
  DEPENDS ${XMLTO_FILES_${mode}}
  VERBATIM
      )
    endif ( XMLTO_ADD_DEFAULT )
  endforeach ( mode )

  set ( XMLTO_MODES )
  set ( XMLTO_FILES )
endmacro ( XMLTO )
