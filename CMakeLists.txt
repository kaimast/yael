cmake_minimum_required (VERSION 2.6)
project (yael)

set(YAEL_SOURCES
    src/network/Socket.cpp src/network/Address.cpp
    src/SocketListener.cpp src/EventLoop.cpp)

set(YAEL_INCLUDES
    include/yael/network/Socket.h include/yael/network/Address.h
    include/yael/SocketListener.h include/yael/EventLoop.h)

set(YAEL_TESTS
    test/main.cpp
    test/SocketTest.h test/SocketTest.cpp
    test/SocketAsyncTest.h test/SocketAsyncTest.cpp
    test/AddressTest.h test/AddressTest.cpp)

add_library(${PROJECT_NAME} ${YAEL_SOURCES} ${YAEL_INCLUDES})
include_directories(. ./include/yael)

add_executable(${PROJECT_NAME}-test ${YAEL_SOURCES} ${YAEL_TESTS})
target_link_libraries(${PROJECT_NAME} glog)
target_link_libraries(${PROJECT_NAME}-test glog gmock pthread)

set(CMAKE_CXX_FLAGS "--std=c++17 -Wall -pedantic-errors -fPIC -Winit-self -Wold-style-cast -Woverloaded-virtual -Wuninitialized -Wmissing-declarations -Wunused-result -O3 -Wdouble-promotion")
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -g -O0")


