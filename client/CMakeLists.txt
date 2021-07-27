cmake_minimum_required (VERSION 3.8)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../base/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third-party/muduo")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../proto")
# include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

file(GLOB DATABASE_SCOURE "${CMAKE_CURRENT_SOURCE_DIR}/*.cc" )


link_libraries(pthread spdlog colib muduo_net muduo_base hiredis_static buffer protobuf protoc Base dl)

add_executable(Client ${DATABASE_SCOURE})