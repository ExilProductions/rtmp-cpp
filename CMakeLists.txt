cmake_minimum_required(VERSION 3.10)
project(rtmp-cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_EXAMPLE_C "Build C example" ON)
option(BUILD_EXAMPLE_CPP "Build C++ example" ON)

include_directories(include)

add_library(rtmp STATIC
    src/rtmp_server.cpp
    src/rtmp_capi.cpp
)

target_include_directories(rtmp PUBLIC 
    BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include;
    INSTALL_INTERFACE:include;
)

set_target_properties(rtmp PROPERTIES
    PUBLIC_HEADER "include/rtmp_capi.hpp;include/rtmp_server.hpp"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    target_link_libraries(rtmp PRIVATE ws2_32)
endif()

if(BUILD_EXAMPLE_C)
    add_executable(rtmp_server_c server/c/main.c)
    target_link_libraries(rtmp_server_c PRIVATE rtmp)
    if(WIN32)
        target_link_libraries(rtmp_server_c PRIVATE ws2_32)
    endif()
endif()

if(BUILD_EXAMPLE_CPP)
    add_executable(rtmp_server_cpp server/cpp/main.cpp)
    target_link_libraries(rtmp_server_cpp PRIVATE rtmp)
    if(WIN32)
        target_link_libraries(rtmp_server_cpp PRIVATE ws2_32)
    endif()
endif()

install(TARGETS rtmp 
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

if(BUILD_EXAMPLE_C)
    install(TARGETS rtmp_server_c DESTINATION bin)
endif()

if(BUILD_EXAMPLE_CPP)
    install(TARGETS rtmp_server_cpp DESTINATION bin)
endif()
