# Ubuntu Blockchain - Production Dockerfile
# Multi-stage build for minimal production image

# ============================================================================
# Stage 1: Builder
# ============================================================================
FROM ubuntu:22.04 AS builder

# Set environment
ENV DEBIAN_FRONTEND=noninteractive
ENV CMAKE_VERSION=3.25.0

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    pkg-config \
    libssl-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-thread-dev \
    libprotobuf-dev \
    protobuf-compiler \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
WORKDIR /opt
RUN git clone https://github.com/Microsoft/vcpkg.git && \
    cd vcpkg && \
    ./bootstrap-vcpkg.sh && \
    ./vcpkg integrate install

# Set vcpkg path
ENV VCPKG_ROOT=/opt/vcpkg

# Create app directory
WORKDIR /app

# Copy source code
COPY . .

# Install dependencies with vcpkg
RUN ${VCPKG_ROOT}/vcpkg install \
    openssl \
    rocksdb \
    boost-system \
    boost-thread \
    boost-asio \
    protobuf \
    spdlog \
    nlohmann-json \
    gtest

# Build Ubuntu Blockchain
RUN mkdir -p build && \
    cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
        -DBUILD_TESTS=OFF \
        -DBUILD_BENCHMARKS=OFF && \
    cmake --build . --config Release -j$(nproc)

# ============================================================================
# Stage 2: Runtime
# ============================================================================
FROM ubuntu:22.04

# Set environment
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libboost-system1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    libprotobuf23 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create ubuntu-blockchain user
RUN groupadd -r ubuntu-blockchain && \
    useradd -r -g ubuntu-blockchain -m -d /home/ubuntu-blockchain ubuntu-blockchain

# Copy binaries from builder
COPY --from=builder /app/build/bin/ubud /usr/local/bin/
COPY --from=builder /app/build/bin/ubu-cli /usr/local/bin/

# Copy configuration example
COPY --from=builder /app/ubuntu.conf.example /etc/ubuntu-blockchain/ubuntu.conf.example

# Create data directory
RUN mkdir -p /var/lib/ubuntu-blockchain && \
    chown -R ubuntu-blockchain:ubuntu-blockchain /var/lib/ubuntu-blockchain

# Create config directory
RUN mkdir -p /etc/ubuntu-blockchain && \
    chown -R ubuntu-blockchain:ubuntu-blockchain /etc/ubuntu-blockchain

# Switch to ubuntu-blockchain user
USER ubuntu-blockchain
WORKDIR /home/ubuntu-blockchain

# Expose ports
# 8333 - P2P
# 8332 - RPC
# 9090 - Metrics
EXPOSE 8333 8332 9090

# Volume for blockchain data
VOLUME ["/var/lib/ubuntu-blockchain"]

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD ubu-cli getblockcount || exit 1

# Start daemon
ENTRYPOINT ["ubud"]
CMD ["-datadir=/var/lib/ubuntu-blockchain", "-conf=/etc/ubuntu-blockchain/ubuntu.conf"]
