# Modern Sniper Simulation Dockerfile
FROM ubuntu:22.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install core dependencies for native C++ CLI and core
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-yaml \
    libsqlite3-dev \
    libcurl4-openssl-dev \
    zlib1g-dev \
    libbz2-dev \
    wget \
    xz-utils \
    vim \
    curl \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set up Sniper environment
ENV SNIPER_ROOT=/opt/sniper
WORKDIR $SNIPER_ROOT

# Copy project files
COPY . $SNIPER_ROOT

# Use bootstrap to fetch dependencies and build
# RUN ./bootstrap.sh

# Default command
CMD ["/bin/bash"]
