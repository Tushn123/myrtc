#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "xrtc-third_party::turbojpeg-static" for configuration "Debug"
set_property(TARGET xrtc-third_party::turbojpeg-static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(xrtc-third_party::turbojpeg-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/turbojpeg-static.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS xrtc-third_party::turbojpeg-static )
list(APPEND _IMPORT_CHECK_FILES_FOR_xrtc-third_party::turbojpeg-static "${_IMPORT_PREFIX}/lib/turbojpeg-static.lib" )

# Import target "xrtc-third_party::jpeg-static" for configuration "Debug"
set_property(TARGET xrtc-third_party::jpeg-static APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(xrtc-third_party::jpeg-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/jpeg-static.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS xrtc-third_party::jpeg-static )
list(APPEND _IMPORT_CHECK_FILES_FOR_xrtc-third_party::jpeg-static "${_IMPORT_PREFIX}/lib/jpeg-static.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
