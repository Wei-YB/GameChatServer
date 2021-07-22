cmake_minimum_required (VERSION 3.8)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../base/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third-party/muduo")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../proto")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

link_libraries(muduo_base muduo_net Base pthread protobuf protoc buffer)

add_executable (ProxyServer "proxy.cc")
