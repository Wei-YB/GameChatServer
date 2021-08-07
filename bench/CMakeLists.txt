cmake_minimum_required (VERSION 3.8)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../base/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third-party/muduo")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../proto")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

link_libraries(pthread muduo_net muduo_base hiredis_static Base buffer protobuf protoc )

add_executable(bench_online "Online.cc")