cmake_minimum_required(VERSION 3.10)

find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development)

message(STATUS "Python_EXECUTABLE=${Python_EXECUTABLE}")
message(STATUS "Python_INCLUDE_DIRS=${Python_INCLUDE_DIRS}")
message(STATUS "Python_LIBRARIES=${Python_LIBRARIES}")
