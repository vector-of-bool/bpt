cmake_minimum_required(VERSION 3.12)
project(TestProject)

include(${PROJECT_BINARY_DIR}/libraries.cmake)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE test::foo)
