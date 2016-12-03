cmake_minimum_required (VERSION 2.6)
project (yael)

set(YAEL_SOURCES
    src/network/Socket.cpp src/network/Address.cpp
    src/SocketListener.cpp src/EventLoop.cpp)

set(YAEL_INCLUDES
    include/yael/yael.h
    include/yael/network/Socket.h include/yael/network/Address.h
    include/yael/SocketListener.h include/yael/EventLoop.h)

set(YAEL_TESTS
    test/main.cpp
    test/SocketTest.h test/SocketTest.cpp
    test/SocketAsyncTest.h test/SocketAsyncTest.cpp
    test/AddressTest.h test/AddressTest.cpp
    test/EventLoopTest.h test/EventLoopTest.cpp)

add_library(${PROJECT_NAME} SHARED ${YAEL_SOURCES} ${YAEL_INCLUDES})
include_directories(. ./include ./include/yael)

add_executable(${PROJECT_NAME}-test ${YAEL_SOURCES} ${YAEL_TESTS})

add_executable(${PROJECT_NAME}-benchmark-client benchmark/Client.cpp benchmark/defines.h)
add_executable(${PROJECT_NAME}-benchmark-server benchmark/Server.cpp benchmark/defines.h)

target_link_libraries(${PROJECT_NAME} glog pthread)
target_link_libraries(${PROJECT_NAME}-test glog gmock pthread)

target_link_libraries(${PROJECT_NAME}-benchmark-client yael glog)
target_link_libraries(${PROJECT_NAME}-benchmark-server yael glog)

set(CMAKE_CXX_FLAGS "--std=c++17 -Wall -pedantic-errors -fPIC -Winit-self -Wold-style-cast -Woverloaded-virtual -Wuninitialized -Wmissing-declarations -Wunused-result -O3 -Wdouble-promotion")
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -g -O0")

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION lib
        PERMISSIONS WORLD_READ WORLD_EXECUTE)

install(DIRECTORY include/yael DESTINATION include)
