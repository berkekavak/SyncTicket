cmake_minimum_required(VERSION 3.16)
project(SyncTicket)

set(CMAKE_CXX_STANDARD 14)

add_executable(SyncTicket SyncTicket.cpp)

target_link_libraries(SyncTicket pthread)