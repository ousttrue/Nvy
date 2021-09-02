cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Nvy)
set(THIRDPARTY_DIR ${CMAKE_CURRENT_LIST_DIR}/third_party)
subdirs(_external src samples)
