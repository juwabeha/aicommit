cmake_minimum_required(VERSION 3.20)
project(aicommit)

set(CMAKE_CXX_STANDARD 23)

add_executable(aicommit main.cpp)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

target_link_libraries(aicommit PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)
