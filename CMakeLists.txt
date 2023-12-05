cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(rid-transmitter VERSION 0.1 LANGUAGES CXX)

string(ASCII 27 Esc)
set(White       "${Esc}[m")
set(Red         "${Esc}[31m")
set(Green       "${Esc}[32m")
set(Yellow      "${Esc}[33m")
set(Blue        "${Esc}[34m")
set(BoldGreen   "${Esc}[1;32m")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-address-of-packed-member")

##### Add libraries
add_subdirectory(libraries)

# Determine git version
exec_program(
    "git"
    ${CMAKE_CURRENT_SOURCE_DIR}
    ARGS "describe"
    OUTPUT_VARIABLE GIT_TAG
 )

#...if not found get the SHA1 hash instead
if(${GIT_TAG} MATCHES "fatal:.*")
    exec_program(
        "git"
        ${CMAKE_CURRENT_SOURCE_DIR}
        ARGS "rev-parse HEAD"
        OUTPUT_VARIABLE GIT_VERSION
    )
else()
    set(GIT_VERSION ${GIT_TAG})
endif()

add_definitions(-DAPP_GIT_VERSION="${GIT_VERSION}")

message("-- Version: ${BoldGreen}${GIT_VERSION}${White}")

include_directories(src/misc)
include_directories(src/Transmitter)
include_directories(src/Transmitter/parameters)

# Create executable
add_executable(rid-transmitter
    src/main.cpp
    src/Transmitter/states/State1.cpp
    src/Transmitter/states/State2.cpp
    src/Transmitter/states/State3.cpp
    src/Transmitter/Transmitter.cpp
    src/Transmitter/parameters/parameters.cpp
)

target_link_libraries(rid-transmitter
    mavlink-cpp
    argparse
)
