project(aspherix_cosim_socket CXX)

cmake_minimum_required(VERSION 3.1)

set(SOURCE aspherix_cosim_socket.h aspherix_cosim_socket.cpp)

add_library(aspherix_cosim_socket SHARED ${SOURCE})

set(STATIC_LIBSTDCPP OFF
  CACHE STRING "enable static linking of libstdc++ and libgcc")

set(CMAKE_CXX_FLAGS "-Wall -std=c++11")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Build type. Can be Debug or Release." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(STATIC_LIBSTDCPP)
  message(STATUS "enabling static linking of libstdc++ and libgcc")
  target_link_libraries(aspherix_cosim_socket -static-libstdc++)
  target_link_libraries(aspherix_cosim_socket -static-libgcc)
endif()

#=============================================================
FIND_PACKAGE(MPI)

IF(MPI_FOUND)
  INCLUDE_DIRECTORIES(${MPI_INCLUDE_PATH})
  target_link_libraries(aspherix_cosim_socket ${MPI_LIBRARIES})

  IF(MPI_COMPILE_FLAGS)
    set_property(TARGET aspherix_cosim_socket
      APPEND PROPERTY COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
  ENDIF()

  IF(MPI_LINK_FLAGS)
    set_property(TARGET aspherix_cosim_socket
      APPEND PROPERTY LINK_FLAGS "${MPI_LINK_FLAGS}")
  ENDIF()

ELSE(MPI_FOUND)
  MESSAGE(SEND_ERROR "MPI not found. Will continue processing cmake, but not build library.")
ENDIF(MPI_FOUND)
#=============================================================
install(TARGETS aspherix_cosim_socket DESTINATION lib)
install(FILES aspherix_cosim_socket.h DESTINATION include/aspherix)
install(FILES aspherix_cosim_socketConfig.cmake DESTINATION include/aspherix/cmake)
