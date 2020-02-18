set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${NOTAVI_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${NOTAVI_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${NOTAVI_BINARY_DIR})

# ##############################################################################
# SOURCE FILES
# ##############################################################################
set(SOURCES main.c)

# ##############################################################################
# TARGET
# ##############################################################################
add_executable(notavi ${SOURCES})
target_include_directories(notavi PUBLIC ${NOTAVI_SOURCE_DIR}/bin)
#set_target_properties(notavi PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED
#                                                        YES CXX_EXTENSIONS OFF)
#target_link_libraries(notavi notavi::notavi)
target_compile_options(notavi PRIVATE ${NOTAVI_CXX_FLAGS})
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_options(notavi PRIVATE ${NOTAVI_CXX_FLAGS_RELEASE})
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(notavi PRIVATE ${NOTAVI_CXX_FLAGS_DEBUG})
endif()

install(TARGETS notavi RUNTIME DESTINATION bin)
