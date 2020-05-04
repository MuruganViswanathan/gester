#
#   Author: Murugan Viswanathan
#   date: 04/21/2020
#   Description: Build ztouch-gestures from source
#   To build using cmake, run the below commands:
#            mkdir build
#            cd build
#            cmake ..
#            make
#

cmake_minimum_required(VERSION 3.0)
project(ztouch-gestures)

set(PROJ_TOUCHGESTURES ztouch-gestures)

file(GLOB SOURCE "*.c" "*.cpp")

#add_executable(${PROJ_TOUCHGESTURES} ${SOURCE})
add_library(ztouch-gestures
    touch-gestures.cpp
)

#add_library(libtouch-gestures SHARED
#    touch-gestures.cpp
#)
#set_target_properties(libtouch-gestures PROPERTIES PUBLIC_HEADER config.h)

target_link_libraries (${PROJ_TOUCHGESTURES} ${CMAKE_THREAD_LIBS_INIT})
#target_link_libraries(${PROJ_TOUCHGESTURES} ${LIBS} ${EXTRA_LIBS})

#install (TARGETS ${PROJ_TOUCHGESTURES} DESTINATION bin)
#install(TARGETS libtouch-gestures
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  
