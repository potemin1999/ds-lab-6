cmake_minimum_required(VERSION 3.14)
project(lab_6)

set(CMAKE_CXX_STANDARD 11)

add_executable(lab_6_client client.cpp)
target_link_libraries(lab_6_client stdc++ pthread)

add_executable(lab_6_server server.cpp)
target_link_libraries(lab_6_server stdc++ pthread)