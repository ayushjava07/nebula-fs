set(TEST_SOURCES
  test_types.cpp
  test_utils_buffer.cpp
  test_utils_varint.cpp
  test_utils_checksum.cpp
  test_utils_memorymapped.cpp
  test_compression_engine.cpp
  test_compression_block.cpp
  test_crypto_encryption.cpp
  test_crypto_hash.cpp
  test_archive_header.cpp
  test_parser.cpp
  test_index_manager.cpp
  test_btree.cpp
  test_hashtable.cpp
  test_chunk_manager.cpp
  test_block_storage.cpp
  test_metadata_store.cpp
  test_entry_metadata.cpp
  test_directory_tree.cpp
  test_file_resolver.cpp
  test_journal_manager.cpp
  test_recovery.cpp
  test_network_server.cpp
  test_integration.cpp
  test_cli.cpp
  test_dashboard_metrics.cpp
)

add_executable(nebula_tests ${TEST_SOURCES})
target_include_directories(nebula_tests PRIVATE ${NEBULA_INCLUDE_DIR})
target_link_libraries(nebula_tests PRIVATE nebula GTest::gtest GTest::gtest_main pthread)

add_test(NAME NebulaTests COMMAND nebula_tests)
