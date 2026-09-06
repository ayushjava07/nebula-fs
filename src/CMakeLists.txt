set(NEBULA_SOURCES
  archive/ArchiveHeader.cpp
  archive/ArchiveWriter.cpp
  archive/ArchiveReader.cpp
  parser/Parser.cpp
  parser/StreamParser.cpp
  parser/MetadataParser.cpp
  compression/CompressionEngine.cpp
  compression/CompressionBlock.cpp
  crypto/EncryptionEngine.cpp
  crypto/HashEngine.cpp
  storage/ChunkManager.cpp
  storage/BlockStorage.cpp
  index/IndexManager.cpp
  index/BTree.cpp
  index/HashTable.cpp
  index/SessionManager.cpp
  metadata/MetadataStore.cpp
  metadata/EntryMetadata.cpp
  filesystem/JournalManager.cpp
  filesystem/Recovery.cpp
  filesystem/DirectoryTree.cpp
  filesystem/FileResolver.cpp
  filesystem/BTree.cpp
  network/ArchiveServer.cpp
  network/ThreadPool.cpp
  network/ProtocolRegistry.cpp
  utils/Buffer.cpp
  utils/VarInt.cpp
  utils/Checksum.cpp
  utils/MemoryMappedFile.cpp
  utils/Allocator.cpp
  utils/SizeCalculator.cpp
  storage/CacheManager.cpp
  cli/CommandLine.cpp
  dashboard/DashboardMetrics.cpp
)

add_library(nebula STATIC ${NEBULA_SOURCES})
target_include_directories(nebula
  PUBLIC
    $<BUILD_INTERFACE:${NEBULA_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${NEBULA_SRC_DIR}
)
target_compile_features(nebula PUBLIC cxx_std_20)
target_compile_definitions(nebula PUBLIC NEBULA_VERSION="${PROJECT_VERSION}")
target_link_libraries(nebula PUBLIC ${NEBULA_LIBS})

if(NEBULA_BUILD_TESTS)
  target_compile_definitions(nebula PUBLIC NEBULA_BUILD_TESTS)
endif()


