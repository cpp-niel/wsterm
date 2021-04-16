cmake_minimum_required(VERSION 3.17)
project(wsterm)

find_package(Curses)

set(CMAKE_CXX_STANDARD 20)

add_executable(wsterm main.cpp)

target_include_directories(wsterm PRIVATE ./)
target_compile_definitions(wsterm PRIVATE _XOPEN_SOURCE_EXTENDED=1)
target_link_libraries(wsterm PRIVATE ${CURSES_LIBRARIES})