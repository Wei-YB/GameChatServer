cmake_minimum_required (VERSION 3.8)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../base/include")

file(GLOB BASE_SCOURE "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc" )

add_library(Base ${BASE_SCOURE})
