cmake_minimum_required(VERSION 3.13)

project(aspherix_cosim_socket VERSION 1.0.0 LANGUAGES CXX)

SET(CMAKE_CXX_STANDARD 20)

SET(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# setting this so that there are no issues with open foam
OPTION(SKIP_MPICXX "Skip mpicxx (recommended for OpenFOAM/Coupling compilations)" ON)
if (${SKIP_MPICXX})
    OPTION(MPI_CXX_SKIP_MPICXX "Skip mpicxx (recommended for OpenFOAM/Coupling compilations)" ON)
    set(CMAKE_CXX_FLAGS "-DMPICH_SKIP_MPICXX -DOMPI_SKIP_MPICXX")
else()
    OPTION(MPI_CXX_SKIP_MPICXX "Skip mpicxx (recommended for OpenFOAM/Coupling compilations)" OFF)
endif()

#set verbose Makefile to double check mpicxx
set(CMAKE_VERBOSE_MAKEFILE ON)

set(SOURCE aspherix_cosim_socket.cpp aspherix_cosim_field.cpp aspherix_cosim_settings.cpp)

OPTION(STATIC_LIBRARY "Compile as static library" OFF)

if (STATIC_LIBRARY)
    add_library(aspherix_cosim_socket STATIC ${SOURCE})
else()
    add_library(aspherix_cosim_socket SHARED ${SOURCE})
endif()

set_property(TARGET aspherix_cosim_socket PROPERTY PUBLIC_HEADER aspherix_cosim_settings.h aspherix_cosim_socket.h aspherix_cosim_field.h aspherix_cosim_interface.h aspherix_cosim_socket_I.h)

OPTION(STATIC_LIBSTDCPP "Build with statically linked C++ standard library" OFF)

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
if (NOT MPI_FOUND)
  FIND_PACKAGE(MPI)
endif()

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

install(TARGETS aspherix_cosim_socket DESTINATION lib
        PUBLIC_HEADER DESTINATION include/aspherix_cosim_socket)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/aspherix_cosim_socketConfig.cmake")
    install(FILES ${SOURCE} DESTINATION include/aspherix_cosim_socket)
    install(FILES CMakeLists.txt DESTINATION include/aspherix_cosim_socket)
    install(FILES compileLib.sh DESTINATION include/aspherix_cosim_socket)
    install(FILES compilerCheck.sh DESTINATION include/aspherix_cosim_socket)
    install(FILES aspherix_cosim_socketConfig.cmake DESTINATION share/aspherix_cosim_socket)
    install(FILES license.txt DESTINATION share/aspherix_cosim_socket)
endif()

# Add Google test
option(COSIM_UNIT_TESTING "Enable unit testing for CoSimSocket" OFF)
if(COSIM_UNIT_TESTING)
  include(FetchContent)
  FetchContent_GetProperties(googletest)
  if(NOT googletest_POPULATED)
    FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    if(WIN32)
      # For Windows: Prevent overriding the parent project's compiler/linker settings
      set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_MakeAvailable(googletest)
  endif()

  enable_testing()
  add_subdirectory(test)
endif()
