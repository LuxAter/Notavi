set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${NOTAVI_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${NOTAVI_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${NOTAVI_BINARY_DIR})

# ##############################################################################
# SOURCE FILES
# ##############################################################################
set(SOURCES
  core.cpp)
configure_file("${NOTAVI_SOURCE_DIR}/include/notavi/version.hpp.in"
               "${NOTAVI_SOURCE_DIR}/include/notavi/version.hpp")

# ##############################################################################
# STATIC TARGET
# ##############################################################################
if(NOTAVI_BUILD_STATIC_LIBS)
  add_library(notavi_static STATIC ${SOURCES})
  add_library(notavi::static ALIAS notavi_static)
  target_include_directories(notavi_static
                             PUBLIC ${NOTAVI_SOURCE_DIR}/include)
  target_include_directories(notavi_static
                             PRIVATE ${NOTAVI_SOURCE_DIR}/include/notavi)

  set_target_properties(
    notavi_static
    PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
  target_compile_options(notavi_shared PRIVATE ${NOTAVI_CXX_FLAGS})
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(notavi_static PRIVATE ${NOTAVI_CXX_FLAGS_RELEASE})
  elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(notavi_static PRIVATE ${NOTAVI_CXX_FLAGS_DEBUG})
  endif()
  install(TARGETS notavi_static ARCHIVE DESTINATION lib)
endif()

# ##############################################################################
# SHARED TARGET
# ##############################################################################
if(NOTAVI_BUILD_SHARED_LIBS)
  add_library(notavi_shared SHARED ${SOURCES})
  add_library(notavi::shared ALIAS notavi_shared)
  target_include_directories(notavi_shared
                             PUBLIC ${NOTAVI_SOURCE_DIR}/include)
  target_include_directories(notavi_shared
                             PRIVATE ${NOTAVI_SOURCE_DIR}/include/notavi)

  set_target_properties(
    notavi_shared
    PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
  target_compile_options(notavi_shared PRIVATE ${NOTAVI_CXX_FLAGS})
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(notavi_shared PRIVATE ${NOTAVI_CXX_FLAGS_RELEASE})
  elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(notavi_shared PRIVATE ${NOTAVI_CXX_FLAGS_DEBUG})
  endif()
  install(TARGETS notavi_shared LIBRARY DESTINATION lib)
endif()

# ##############################################################################
# ALIASING
# ##############################################################################
if(NOTAVI_BUILD_SHARED_LIBS)
  add_library(notavi::notavi ALIAS notavi_shared)
elseif(NOTAVI_BUILD_STATIC_LIBS)
  add_library(notavi::notavi ALIAS notavi_static)
endif()
