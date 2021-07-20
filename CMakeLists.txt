cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 17)

project ("GameChatServer")

add_library(buffer "proto/message.pb.cc")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third-party/muduo")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/base")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/chat")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/database")

# add_executable (GameChatServer "GameChatServer.cpp" "GameChatServer.h" )

# TODO: 如有需要，请添加测试并安装目标。
