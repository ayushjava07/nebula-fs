cmake_minimum_required(VERSION 3.16)

# Build zlib from bundled source
set(ZLIB_SOURCES
    zlib/adler32.c
    zlib/compress.c
    zlib/crc32.c
    zlib/deflate.c
    zlib/gzclose.c
    zlib/gzlib.c
    zlib/gzread.c
    zlib/gzwrite.c
    zlib/infback.c
    zlib/inffast.c
    zlib/inflate.c
    zlib/inftrees.c
    zlib/trees.c
    zlib/uncompr.c
    zlib/zutil.c
)
add_library(bundled_zlib STATIC ${ZLIB_SOURCES})
target_include_directories(bundled_zlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/zlib)
target_compile_definitions(bundled_zlib PRIVATE -DHAVE_HIDDEN)

# Build lz4 from bundled source
set(LZ4_SOURCES
    lz4/lib/lz4.c
    lz4/lib/lz4hc.c
    lz4/lib/lz4frame.c
    lz4/lib/xxhash.c
)
add_library(bundled_lz4 STATIC ${LZ4_SOURCES})
target_include_directories(bundled_lz4 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lz4/lib)

# Build zstd from bundled source
set(ZSTD_SOURCES
    zstd/lib/common/debug.c
    zstd/lib/common/entropy_common.c
    zstd/lib/common/error_private.c
    zstd/lib/common/fse_decompress.c
    zstd/lib/common/pool.c
    zstd/lib/common/threading.c
    zstd/lib/common/xxhash.c
    zstd/lib/common/zstd_common.c
    zstd/lib/compress/fse_compress.c
    zstd/lib/compress/hist.c
    zstd/lib/compress/huf_compress.c
    zstd/lib/compress/zstd_compress.c
    zstd/lib/compress/zstd_compress_literals.c
    zstd/lib/compress/zstd_compress_sequences.c
    zstd/lib/compress/zstd_compress_superblock.c
    zstd/lib/compress/zstd_double_fast.c
    zstd/lib/compress/zstd_fast.c
    zstd/lib/compress/zstd_lazy.c
    zstd/lib/compress/zstd_ldm.c
    zstd/lib/compress/zstd_opt.c
    zstd/lib/compress/zstdmt_compress.c
    zstd/lib/decompress/huf_decompress.c
    zstd/lib/decompress/zstd_ddict.c
    zstd/lib/decompress/zstd_decompress.c
    zstd/lib/decompress/zstd_decompress_block.c
    zstd/lib/deprecated/zbuff_common.c
    zstd/lib/deprecated/zbuff_compress.c
    zstd/lib/deprecated/zbuff_decompress.c
    zstd/lib/dictBuilder/cover.c
    zstd/lib/dictBuilder/fastcover.c
    zstd/lib/dictBuilder/divsufsort.c
    zstd/lib/dictBuilder/zdict.c
)
add_library(bundled_zstd STATIC ${ZSTD_SOURCES})
target_include_directories(bundled_zstd
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/lib/common
)
target_compile_definitions(bundled_zstd PRIVATE -DXXH_NAMESPACE=ZSTD_ -DZSTD_DISABLE_ASM)

# Build standalone SHA256 (replaces OpenSSL EVP for checksumming)
add_library(bundled_sha256 STATIC sha256/sha256.c)
target_include_directories(bundled_sha256 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sha256)
