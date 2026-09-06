FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install Build Dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    libmysqlclient-dev \
    libboost-all-dev \
    libcrypto++-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set Working Directory
WORKDIR /app

# Copy Dependencies and Reality Source
COPY Server/Dependencies10 ./Server/Dependencies10
COPY Server/Reality ./Server/Reality

# Build Reality Server
WORKDIR /app/Server/Reality
RUN mkdir -p build && cd build && \
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .. && \
    ninja Reality && \
    mkdir -p /app/Server/Reality/Binaries/Data && \
    cp -rn /app/Server/Reality/Data/* /app/Server/Reality/Binaries/Data/ 2>/dev/null || true

# Expose Game and Auth Ports
EXPOSE 10000/udp 10000/tcp 11000/tcp

# Working directory for running Reality
WORKDIR /app/Server/Reality/Binaries

CMD ["./Reality"]
