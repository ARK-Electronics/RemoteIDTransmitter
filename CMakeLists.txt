cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(rid-transmitter VERSION 0.1 LANGUAGES C CXX)

add_compile_options(-Wall -Werror -Wunused -Wextra -Wno-address-of-packed-member)

string(ASCII 27 Esc)
set(White       "${Esc}[m")
set(Red         "${Esc}[31m")
set(Green       "${Esc}[32m")
set(Yellow      "${Esc}[33m")
set(Blue        "${Esc}[34m")
set(BoldGreen   "${Esc}[1;32m")

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

##### Build mavlink-cpp
add_subdirectory(libraries/mavlink-cpp)

# Include bluez
find_package(PkgConfig REQUIRED)
pkg_check_modules(BLUEZ REQUIRED bluez)
include_directories(${BLUEZ_INCLUDE_DIRS})
link_directories(${BLUEZ_LIBRARY_DIRS})

include_directories(
    src/misc
    src/Bluetooth
    src/Transmitter
    src/Transmitter/parameters
    libraries/tomlplusplus/
    libraries/opendroneid-core-c/libopendroneid
    ${BLUEZ_INCLUDE_DIRS}
)

# Create executable
add_executable(${PROJECT_NAME}
    libraries/opendroneid-core-c/libopendroneid/opendroneid.c
    src/Bluetooth/Bluetooth.cpp
    src/Bluetooth/BluetoothLegacy.cpp
    src/Bluetooth/print_bt_features.c
    src/Transmitter/Transmitter.cpp
    src/Transmitter/parameters/parameters.cpp
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    mavlink-cpp
    ${BLUEZ_LIBRARIES}
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

install(DIRECTORY DESTINATION $ENV{HOME}/.local/share/${PROJECT_NAME}
    DIRECTORY_PERMISSIONS
    OWNER_WRITE OWNER_READ OWNER_EXECUTE
    GROUP_WRITE GROUP_READ GROUP_EXECUTE
    WORLD_WRITE WORLD_READ WORLD_EXECUTE)

install(FILES config.toml DESTINATION $ENV{HOME}/.local/share/${PROJECT_NAME})
install(CODE "execute_process(COMMAND ./set_capabilities.sh)")
