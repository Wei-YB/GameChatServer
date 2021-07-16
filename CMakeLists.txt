cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 17)

project ("GameChatServer")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third-party/muduo")


add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/base")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/proxy")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/database")

add_executable (GameChatServer "GameChatServer.cpp" "GameChatServer.h" "UUIDGenerator.h")

add_executable(UUIDGenerator UUIDGenerator.cc)


# TODO: 如有需要，请添加测试并安装目标。
