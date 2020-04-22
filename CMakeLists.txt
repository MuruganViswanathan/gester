#
#   Author: Murugan Viswanathan
#   date: 04/21/2020
#

cmake_minimum_required(VERSION 3.0)
project(ztouch-gestures)

set(PROJ_TOUCHGESTURES ztouch-gestures)
file(GLOB SOURCES "*.c" "*.cpp")

add_executable(ztouch-gestures ${SOURCES})

target_link_libraries (${PROJ_TOUCHGESTURES}
    ${CMAKE_THREAD_LIBS_INIT}
    )

install (TARGETS ${PROJ_TOUCHGESTURES} DESTINATION bin)

