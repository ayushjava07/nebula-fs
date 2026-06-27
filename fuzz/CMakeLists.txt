cmake_minimum_required(VERSION 3.22)

set(FUZZ_SOURCES
  archive_parser_fuzzer.cpp
  index_fuzzer.cpp
  compression_fuzzer.cpp
  journal_fuzzer.cpp
  metadata_fuzzer.cpp
)

set(FUZZ_LIBS nebula ${NEBULA_LIBS})

# Allow the fuzzer engine flag to be overridden (e.g. by ClusterFuzzLite).
# Default to libFuzzer for local builds.
if(NOT FUZZER_ENGINE_FLAG)
  set(FUZZER_ENGINE_FLAG "-fsanitize=fuzzer")
endif()

foreach(FUZZ_SRC ${FUZZ_SOURCES})
  get_filename_component(FUZZ_NAME ${FUZZ_SRC} NAME_WE)
  add_executable(${FUZZ_NAME} ${FUZZ_SRC})
  target_include_directories(${FUZZ_NAME} PRIVATE ${NEBULA_INCLUDE_DIR})
  target_link_libraries(${FUZZ_NAME} PRIVATE ${FUZZ_LIBS})
  target_compile_options(${FUZZ_NAME} PRIVATE ${FUZZER_ENGINE_FLAG})
  target_link_options(${FUZZ_NAME} PRIVATE ${FUZZER_ENGINE_FLAG})
endforeach()
