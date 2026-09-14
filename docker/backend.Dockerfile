FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config \
    libgrpc++-dev protobuf-compiler protobuf-compiler-grpc libprotobuf-dev \
    libsqlite3-dev libboost-system-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

RUN mkdir -p /etc/darwinsim /var/lib/darwinsim \
    && cp config/default.yaml /etc/darwinsim/default.yaml

ENV DARWINSIM_CONFIG=/etc/darwinsim/default.yaml
ENV DARWINSIM_DATA_DIR=/var/lib/darwinsim

CMD ["/src/build/darwinsim-controller"]
