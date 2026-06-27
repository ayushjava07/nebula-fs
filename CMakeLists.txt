cmake_minimum_required(VERSION 3.22)
project(NebulaFS
  VERSION 1.0.0
  DESCRIPTION "Lightweight embedded binary object database and archive format"
  LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wformat=2 -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -march=native -flto")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g -DNDEBUG")

option(NEBULA_BUILD_TESTS "Build unit tests" ON)
option(NEBULA_BUILD_FUZZ "Build fuzz targets" OFF)
option(NEBULA_BUILD_DOCS "Build documentation" OFF)
option(NEBULA_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(NEBULA_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(NEBULA_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(NEBULA_ENABLE_ASAN)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
endif()
if(NEBULA_ENABLE_UBSAN)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
endif()
if(NEBULA_ENABLE_TSAN)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
endif()

# Find required system libraries
find_package(PkgConfig QUIET)

# ZLIB
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
  find_library(ZLIB_LIBRARIES z)
  find_path(ZLIB_INCLUDE_DIRS zlib.h)
  if(ZLIB_LIBRARIES AND ZLIB_INCLUDE_DIRS)
    add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
      IMPORTED_LOCATION "${ZLIB_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}")
    set(ZLIB_FOUND TRUE)
  endif()
endif()
if(NOT ZLIB_FOUND)
  message(FATAL_ERROR "ZLIB not found. Install zlib1g-dev or equivalent.")
endif()

# OpenSSL
find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND)
  find_library(OPENSSL_SSL_LIBRARY ssl)
  find_library(OPENSSL_CRYPTO_LIBRARY crypto)
  find_path(OPENSSL_INCLUDE_DIR openssl/ssl.h)
  if(OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY AND OPENSSL_INCLUDE_DIR)
    add_library(OpenSSL::SSL UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
      IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
      IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    set(OpenSSL_FOUND TRUE)
  endif()
endif()
if(NOT OpenSSL_FOUND)
  message(FATAL_ERROR "OpenSSL not found. Install libssl-dev or equivalent.")
endif()

# LZ4
find_package(lz4 QUIET)
if(NOT lz4_FOUND)
  find_library(LZ4_LIBRARY lz4)
  find_path(LZ4_INCLUDE_DIR lz4.h)
  if(LZ4_LIBRARY AND LZ4_INCLUDE_DIR)
    add_library(lz4::lz4 UNKNOWN IMPORTED)
    set_target_properties(lz4::lz4 PROPERTIES
      IMPORTED_LOCATION "${LZ4_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${LZ4_INCLUDE_DIR}")
    set(lz4_FOUND TRUE)
  endif()
endif()
if(NOT lz4_FOUND)
  message(FATAL_ERROR "LZ4 not found. Install liblz4-dev or equivalent.")
endif()

# Zstd
find_package(zstd QUIET)
if(NOT zstd_FOUND)
  find_library(ZSTD_LIBRARY zstd)
  find_path(ZSTD_INCLUDE_DIR zstd.h)
  if(ZSTD_LIBRARY AND ZSTD_INCLUDE_DIR)
    add_library(zstd::zstd UNKNOWN IMPORTED)
    set_target_properties(zstd::zstd PROPERTIES
      IMPORTED_LOCATION "${ZSTD_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${ZSTD_INCLUDE_DIR}")
    set(zstd_FOUND TRUE)
  endif()
endif()
if(NOT zstd_FOUND)
  message(FATAL_ERROR "zstd not found. Install libzstd-dev or equivalent.")
endif()

set(NEBULA_LIBS
  ZLIB::ZLIB
  OpenSSL::SSL
  OpenSSL::Crypto
  lz4::lz4
  zstd::zstd
)

set(NEBULA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(NEBULA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_subdirectory(src)

if(NEBULA_BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  add_subdirectory(tests)
endif()

if(NEBULA_BUILD_FUZZ)
  add_subdirectory(fuzz)
endif()

add_custom_target(lint
  COMMAND clang-tidy ${NEBULA_SRC_DIR}/*.cpp ${NEBULA_SRC_DIR}/**/*.cpp
    -header-filter=${NEBULA_INCLUDE_DIR}/** -- -std=c++20
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Running clang-tidy on source files"
)

include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS nebula ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
