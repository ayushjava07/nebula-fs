cmake_minimum_required(VERSION 3.16)
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

# ---- Dependencies: try system, fall back to bundled ----

# ZLIB
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
  find_library(ZLIB_LIBRARIES z)
  find_path(ZLIB_INCLUDE_DIRS zlib.h)
endif()
if(ZLIB_FOUND OR (ZLIB_LIBRARIES AND ZLIB_INCLUDE_DIRS))
  if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
      IMPORTED_LOCATION "${ZLIB_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}")
  endif()
  set(NEBULA_USE_BUNDLED_ZLIB OFF)
else()
  set(NEBULA_USE_BUNDLED_ZLIB ON)
endif()

# OpenSSL
find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND)
  find_library(OPENSSL_SSL_LIBRARY ssl)
  find_library(OPENSSL_CRYPTO_LIBRARY crypto)
  find_path(OPENSSL_INCLUDE_DIR openssl/ssl.h)
endif()
if(OpenSSL_FOUND OR (OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY AND OPENSSL_INCLUDE_DIR))
  if(NOT TARGET OpenSSL::SSL)
    add_library(OpenSSL::SSL UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
      IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
    add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
      IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
  endif()
  add_definitions(-DNEBULA_HAS_OPENSSL)
  set(NEBULA_USE_BUNDLED_OPENSSL OFF)
else()
  set(NEBULA_USE_BUNDLED_OPENSSL ON)
endif()

# LZ4
find_package(lz4 QUIET)
if(NOT lz4_FOUND)
  find_library(LZ4_LIBRARY lz4)
  find_path(LZ4_INCLUDE_DIR lz4.h)
endif()
if(lz4_FOUND OR (LZ4_LIBRARY AND LZ4_INCLUDE_DIR))
  if(NOT TARGET lz4::lz4)
    add_library(lz4::lz4 UNKNOWN IMPORTED)
    set_target_properties(lz4::lz4 PROPERTIES
      IMPORTED_LOCATION "${LZ4_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${LZ4_INCLUDE_DIR}")
  endif()
  set(NEBULA_USE_BUNDLED_LZ4 OFF)
else()
  set(NEBULA_USE_BUNDLED_LZ4 ON)
endif()

# Zstd
find_package(zstd QUIET)
if(NOT zstd_FOUND)
  find_library(ZSTD_LIBRARY zstd)
  find_path(ZSTD_INCLUDE_DIR zstd.h)
endif()
if(TARGET zstd::libzstd_static)
  add_library(zstd::zstd INTERFACE IMPORTED)
  target_link_libraries(zstd::zstd INTERFACE zstd::libzstd_static)
  set(NEBULA_USE_BUNDLED_ZSTD OFF)
elseif(TARGET zstd::libzstd_shared)
  add_library(zstd::zstd INTERFACE IMPORTED)
  target_link_libraries(zstd::zstd INTERFACE zstd::libzstd_shared)
  set(NEBULA_USE_BUNDLED_ZSTD OFF)
elseif(ZSTD_LIBRARY AND ZSTD_INCLUDE_DIR)
  if(NOT TARGET zstd::zstd)
    add_library(zstd::zstd UNKNOWN IMPORTED)
    set_target_properties(zstd::zstd PROPERTIES
      IMPORTED_LOCATION "${ZSTD_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${ZSTD_INCLUDE_DIR}")
  endif()
  set(NEBULA_USE_BUNDLED_ZSTD OFF)
else()
  set(NEBULA_USE_BUNDLED_ZSTD ON)
endif()

# Fall back to bundled deps if any system libs were missing
if(NEBULA_USE_BUNDLED_ZLIB OR NEBULA_USE_BUNDLED_LZ4 OR NEBULA_USE_BUNDLED_ZSTD OR NEBULA_USE_BUNDLED_OPENSSL)
  add_subdirectory(third_party)
  message(STATUS "Using bundled dependencies for some libraries")
else()
  message(STATUS "Using system dependencies")
endif()

# Build target lists for NEBULA_LIBS
set(NEBULA_LIBS)
if(NEBULA_USE_BUNDLED_ZLIB)
  list(APPEND NEBULA_LIBS bundled_zlib)
  list(APPEND NEBULA_LIBS_BUNDLED bundled_zlib)
else()
  list(APPEND NEBULA_LIBS ZLIB::ZLIB)
endif()

if(NEBULA_USE_BUNDLED_LZ4)
  list(APPEND NEBULA_LIBS bundled_lz4)
  list(APPEND NEBULA_LIBS_BUNDLED bundled_lz4)
else()
  list(APPEND NEBULA_LIBS lz4::lz4)
endif()

if(NEBULA_USE_BUNDLED_ZSTD)
  list(APPEND NEBULA_LIBS bundled_zstd)
  list(APPEND NEBULA_LIBS_BUNDLED bundled_zstd)
else()
  list(APPEND NEBULA_LIBS zstd::zstd)
endif()

if(NEBULA_USE_BUNDLED_OPENSSL)
  list(APPEND NEBULA_LIBS bundled_sha256)
  list(APPEND NEBULA_LIBS_BUNDLED bundled_sha256)
else()
  list(APPEND NEBULA_LIBS OpenSSL::SSL OpenSSL::Crypto)
endif()

set(NEBULA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(NEBULA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_subdirectory(src)

# CLI Binary
add_executable(nebula_cli cmd/nebula/main.cpp)
set_target_properties(nebula_cli PROPERTIES OUTPUT_NAME "nebula")
target_include_directories(nebula_cli PRIVATE ${NEBULA_INCLUDE_DIR})
target_link_libraries(nebula_cli PRIVATE nebula)

if(NEBULA_BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/_deps/googletest-src")
    FetchContent_Declare(
      googletest
      SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/googletest-src"
    )
  else()
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.14.0
    )
  endif()
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
