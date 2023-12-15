cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(rid-transmitter VERSION 0.1 LANGUAGES C CXX)

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

include_directories(
    src/
    src/misc
    src/Bluetooth
    src/Transmitter
    src/Transmitter/parameters
    libraries/opendroneid-core-c/libopendroneid
    libraries/bluez
)

# Create executable
add_executable(rid-transmitter
    libraries/opendroneid-core-c/libopendroneid/opendroneid.c
    libraries/bluez/lib/hci.c
    libraries/bluez/lib/bluetooth.c
    src/Bluetooth/Bluetooth.cpp
    src/Bluetooth/print_bt_features.c
    src/Transmitter/Transmitter.cpp
    src/Transmitter/parameters/parameters.cpp
    src/main.cpp
)

target_link_libraries(rid-transmitter
    mavlink-cpp
    argparse
)
