#!/bin/bash -eu
#
# Build script for ClusterFuzzLite / Fenrir.
# Builds all fuzz targets using CFL environment variables.
#
# Environment (provided by CFL):
#   SRC    - source root directory
#   OUT    - output directory for fuzz targets
#   CC, CXX, CFLAGS, CXXFLAGS
#   LIB_FUZZING_ENGINE

# Find the project root.
if [[ -n "${SRC:-}" && -f "${SRC}/CMakeLists.txt" ]]; then
    PROJECT_DIR="${SRC}"
elif [[ -f "$(dirname "${BASH_SOURCE[0]}")/CMakeLists.txt" ]]; then
    PROJECT_DIR="$(dirname "${BASH_SOURCE[0]}")"
elif [[ -f "$(pwd)/CMakeLists.txt" ]]; then
    PROJECT_DIR="$(pwd)"
else
    echo "Error: cannot find CMakeLists.txt"
    exit 1
fi

BUILD_DIR="${PROJECT_DIR}/build_fuzz"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake "${PROJECT_DIR}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER="${CC}" \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_C_FLAGS="${CFLAGS:-} -g -O1" \
    -DCMAKE_CXX_FLAGS="${CXXFLAGS:-} -g -O1" \
    -DFUZZER_ENGINE_FLAG="${LIB_FUZZING_ENGINE:--fsanitize=fuzzer}" \
    -DNEBULA_BUILD_TESTS=OFF \
    -DNEBULA_BUILD_FUZZ=ON

make -j"$(nproc)"

# Copy fuzz targets to $OUT
for fuzz_target in archive_parser_fuzzer index_fuzzer compression_fuzzer journal_fuzzer metadata_fuzzer; do
    if [[ -f "${BUILD_DIR}/fuzz/${fuzz_target}" ]]; then
        cp "${BUILD_DIR}/fuzz/${fuzz_target}" "${OUT}/"
    else
        echo "Warning: ${fuzz_target} not found"
    fi
done

# Per-harness seed corpora
SEED_DIR="${PROJECT_DIR}/fuzz/corpus"
if [[ -d "${SEED_DIR}" ]]; then
    for fuzz_target in archive_parser_fuzzer index_fuzzer compression_fuzzer journal_fuzzer metadata_fuzzer; do
        target_seed="${SEED_DIR}/${fuzz_target}"
        if [[ -d "${target_seed}" ]]; then
            mkdir -p "${OUT}/seeds/${fuzz_target}"
            cp -r "${target_seed}/"* "${OUT}/seeds/${fuzz_target}/" 2>/dev/null || true
        fi
    done
fi

echo "Build complete."
ls -la "${OUT}/" 2>/dev/null || true
