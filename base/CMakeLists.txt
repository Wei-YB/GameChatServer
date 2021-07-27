cmake_minimum_required (VERSION 3.8)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third-party/muduo")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../proto")

link_libraries(pthread muduo_net muduo_base protobuf protoc hiredis_static)

file(GLOB BASE_SCOURE "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc" )

add_library(Base ${BASE_SCOURE})
