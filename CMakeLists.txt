# ##############################################################################
# PROJECT
# ##############################################################################
cmake_minimum_required(VERSION 3.11.0 FATAL_ERROR)
project(Notavi VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# ##############################################################################
# OPTIONS
# ##############################################################################
option(NOTAVI_BUILD_BINARY "Compiles binary executable" TRUE)
option(NOTAVI_BUILD_DOCUMENTATION "Compiles documentation" TRUE)
if(NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  option(NOTAVI_BUILD_SHARED_LIBS "Compile shared libraries" TRUE)
  option(NOTAVI_BUILD_STATIC_LIBS "Compile static libraries" FALSE)
else()
  option(NOTAVI_BUILD_STATIC_LIBS "Compile static libraries" TRUE)
  set(NOTAVI_BUILD_SHARED_LIBS FALSE)
endif()

option(NOTAVI_STATIC_ANALYSIS "Use static analysis tools" TRUE)
set(NOTAVI_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(NOTAVI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ##############################################################################
# BUILD MODE
# ##############################################################################
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      "Debug"
      CACHE STRING "Build Mode" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
  if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
              ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json)
  endif()
endif()
set(NOTAVI_CXX_FLAGS "-Wall")
set(NOTAVI_CXX_FLAGS_DEBUG "-DNOTAVI_DEBUG")
set(NOTAVI_CXX_FLAGS_RELEASE "-O3")
if(NOT MSVC)
  list(APPEND NOTAVI_CXX_FLAGS "-Wextra")
else()
  list(APPEND NOTAVI_CXX_FLAGS "-DNOMINMAX")
endif()

# ##############################################################################
# STATIC ANALYSIS
# ##############################################################################
if(NOTAVI_STATIC_ANALYSIS OR CMAKE_BUILD_TYPE STREQUAL "Release")
  find_program(
    CLANG_TIDY_EXE
    NAMES "clang-tidy"
    DOC "Path to clang tidy exe")
  if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()
  find_program(CPPCHECK_EXE NAME "cppcheck" DOC "Path to cppcheck exe")
  if(CPPCHECK_EXE)
    set(CMAKE_CXX_CPPCHECK "${CPPCHECK_EXE}" "--force")
  endif()
  find_program(CPPLINT_EXE "cpplint")
  if(CPPLINT_EXE)
    set(CMAKE_CXX_CPPLINT "${CPPLINT_EXE}")
  endif()
endif()

# ##############################################################################
# DEPENDENCIES
# ##############################################################################
include(WriteCompilerDetectionHeader)
add_subdirectory(external)
set(NOTAVI_COMPILERS MSVC GNU Clang Intel AppleClang)
write_compiler_detection_header(
  FILE
  ${CMAKE_CURRENT_SOURCE_DIR}/include/notavi/compiler.hpp
  PREFIX
  NOTAVI
  COMPILERS
  ${NOTAVI_COMPILERS}
  FEATURES
  cxx_constexpr
  cxx_noexcept
  cxx_static_assert
  cxx_attribute_deprecated)

# ##############################################################################
# LIBRARY
# ##############################################################################
add_subdirectory(lib)
add_subdirectory(bin)

# ##############################################################################
# DOCUMENTATION
# ##############################################################################
if(NOTAVI_BUILD_DOCUMENTATION)
  find_package(Doxygen)
  # if(Doxygen_FOUND)
    # add_subdirectory(docs)
  # endif()
endif()

# ##############################################################################
# PACKAGING
# ##############################################################################
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CPACK_GENERATOR "TGZ;ZIP")
  set(CPACK_PACKAGE_VENDOR "LuxAter")
  set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
  set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
  set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
  set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CMAKE_SYSTEM_NAME}")
  set(CPACK_MONOLITHIC_INSTALL TRUE)
  include(CPack)
endif()
