cmake_minimum_required(VERSION 2.8.11)
project (cpsumoncli)

include_directories(include)

add_subdirectory (libcpsumon)

add_executable(cpsumoncli cpsumoncli.c)

target_link_libraries (cpsumoncli cpsumon m)

