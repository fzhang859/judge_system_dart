cmake_minimum_required(VERSION 3.0.0)
project(judge)

set(CMAKE_CXX_STANDARD 17)

include_directories(${PROJECT_SOURCE_DIR}/include)

file(GLOB MAIN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(app ${MAIN_SRC})
