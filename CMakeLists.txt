# CMakeLists template for Raspberry Pi Pico projects
#

# Set your target app name here
set(APP_NAME pico-test-i2c)

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Set CMake defines for the Raspberry Pi Pico and required components
set(TARGET_PICO on)
set(HAVE_I2C on)
set(HAVE_SPI off)
set(HAVE_LED on)
set(HAVE_PWM off)

#
# The pico-sdk location is taken from the PICO_SDK_PATH environment variable
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
# If this is not set, you can set it here
# set(PICO_SDK_PATH "/home/user/pico-sdk")

#
# The CppRaspberry library location is taken from the CPP_RASPBERRY_PATH environment variable
# set(CPP_RASPBERRY_PATH $ENV{CPP_RASPBERRY_PATH})
# If this is not set, you can set it here
set(CPP_RASPBERRY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../CppRaspberry")

set(PICO_BOARD pico CACHE STRING "Board type")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(${APP_NAME} C CXX ASM)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# include the CppRaspberry library
include(${CPP_RASPBERRY_PATH}/CppRaspberry.cmake)

# And now our local project
add_executable(${APP_NAME} ${CMAKE_CURRENT_LIST_DIR}/src/pico-test-i2c.cpp)

# Add CppRaspberry library
add_cpp_raspberry_app(${APP_NAME})

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(${APP_NAME})

pico_set_program_name(${APP_NAME} "${APP_NAME}")
pico_set_program_version(${APP_NAME} "0.1")

pico_enable_stdio_uart(${APP_NAME} 0)
pico_enable_stdio_usb(${APP_NAME} 1)

# Test build: wait for a pulse on GP2 before asking the bus controller for an address.
option(TEST_TRIGGER "Send the first address request on a trigger pulse" OFF)
if(TEST_TRIGGER)
  target_compile_definitions(${APP_NAME} PRIVATE TEST_TRIGGER)
endif()

# Test builds for the address confirmation: lose the first N SetAddress messages (a high N: never take over an address),
# and optionally ask for an address only once.
set(TEST_IGNORE_SETADDRESS 0 CACHE STRING "Ignore this many SetAddress messages")
if(TEST_IGNORE_SETADDRESS GREATER 0)
  target_compile_definitions(${APP_NAME} PRIVATE TEST_IGNORE_SETADDRESS=${TEST_IGNORE_SETADDRESS})
endif()
option(TEST_NO_RETRY "Ask for an address only once (test build)" OFF)
if(TEST_NO_RETRY)
  target_compile_definitions(${APP_NAME} PRIVATE TEST_NO_RETRY)
endif()

target_include_directories(${APP_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

target_compile_options(${APP_NAME} PRIVATE -Wall -Wextra -Werror)

target_link_libraries(${APP_NAME} pico_stdlib pico_stdio_usb)
