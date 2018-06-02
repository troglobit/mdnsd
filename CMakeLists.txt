cmake_minimum_required(VERSION 2.8)

project(mdnsd)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/tools/cmake")

add_subdirectory(libmdnsd)

#option(MDNSD_BUILD_UNIT_TESTS "Build the unit tests" OFF)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_BINARY_DIR}/libmdnsd)
add_executable(mdnsd mdnsd.c)
target_link_libraries(mdnsd libmdnsd)
if(WIN32 OR MINGW)
  target_link_libraries(mdnsd wsock32 ws2_32)
endif()

add_executable(mquery mquery.c)
target_link_libraries(mquery libmdnsd)
if(WIN32 OR MINGW)
  target_link_libraries(mquery wsock32 ws2_32)
endif()


#if(MDNSD_BUILD_UNIT_TESTS)
#  enable_testing()
#  add_subdirectory(tests)
#endif()
