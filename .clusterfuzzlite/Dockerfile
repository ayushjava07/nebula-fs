FROM gcr.io/oss-fuzz-base/base-builder
RUN apt-get update && apt-get install -y --no-install-recommends \
    zlib1g-dev libssl-dev liblz4-dev libzstd-dev \
    && rm -rf /var/lib/apt/lists/*
