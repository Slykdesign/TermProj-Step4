cmake_minimum_required(VERSION 3.30)
project(TermProject C)

set(CMAKE_C_STANDARD 23)

add_executable(TermProject main.c vdi.h vdi.c partition.h partition.c ext2.h ext2.c)