cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 17)

project ("GameChatServer")

add_library(buffer "proto/message.pb.cc")

SET(CMAKE_BUILD_TYPE "Release")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third-party/muduo")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/base")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/chat")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/database")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/proxy")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/client")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/bench")

# add_executable (GameChatServer "GameChatServer.cpp" "GameChatServer.h" )

# TODO: 如有需要，请添加测试并安装目标。
