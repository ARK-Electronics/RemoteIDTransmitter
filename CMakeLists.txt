cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(rid-transmitter VERSION 0.1 LANGUAGES C CXX)

add_compile_options(-Wall -Werror -Wunused -Wextra -Wno-address-of-packed-member)

# Assumes MAVSDK system wide install
list(APPEND CMAKE_PREFIX_PATH "/usr/local/MAVSDK/install")
find_package(MAVSDK 3 REQUIRED)

# Include bluez
find_package(PkgConfig REQUIRED)
pkg_check_modules(BLUEZ REQUIRED IMPORTED_TARGET bluez)

# Create executable
add_executable(${PROJECT_NAME}
    libraries/opendroneid-core-c/libopendroneid/opendroneid.c
    src/Bluetooth/Bluetooth.cpp
    src/Bluetooth/BluetoothLegacy.cpp
    src/Bluetooth/print_bt_features.c
    src/Transmitter/Transmitter.cpp
    src/main.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src/misc
    src/Bluetooth
    src/Transmitter
    libraries/tomlplusplus/
    libraries/opendroneid-core-c/libopendroneid
)

target_link_libraries(${PROJECT_NAME}
    MAVSDK::mavsdk
    PkgConfig::BLUEZ
)
