cmake_minimum_required (VERSION 3.8)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../base/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third-party/muduo")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../proto")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

file(GLOB DATABASE_SCOURE "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc" )


link_libraries(pthread muduo_net muduo_base hiredis_static Base buffer protobuf protoc )

add_executable(DatabaseServer ${DATABASE_SCOURE})
add_executable(DatabaseBench ${CMAKE_CURRENT_SOURCE_DIR}/bench/bench.cc)