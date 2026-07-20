# HCScoin: Dockerfile for building and running the daemon.
# Based on Ubuntu 22.04 LTS.

FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential cmake libevent-dev libsqlite3-dev \
    libboost-dev libboost-multi-index-dev \
    cargo rustc pkg-config \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . /build

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_GUI=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON \
    -DENABLE_EXTERNAL_SIGNER=OFF -DBUILD_TESTS=OFF \
  && cmake --build build -j$(nproc)

# Build Panta-Sim quantum backend
RUN cd panta-sim && cargo build --release && cd ..

# Pruned runtime image
FROM ubuntu:22.04 AS runtime
RUN apt-get update && apt-get install -y \
    libevent-2.1.7 libsqlite3-0 libboost-filesystem1.74.0 \
    libboost-thread1.74.0 libboost-chrono1.74.0 \
    libstdc++6 ca-certificates \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /root/
COPY --from=builder /build/build/src/hcscoind /usr/local/bin/
COPY --from=builder /build/build/src/hcscoin-cli /usr/local/bin/
COPY --from=builder /build/panta-sim/target/release/libpanta_sim.so /usr/local/lib/
COPY --from=builder /build/panta-sim/target/release/panta_sim.h /usr/local/include/

RUN ldconfig

EXPOSE 28333 28332 28334 28336 28338 28339

ENV HCSCOIN_PANTA_SIM=1
ENV HCSCOIN_DATA=/root/.hcscoin

VOLUME ["/root/.hcscoin"]

ENTRYPOINT ["hcscoind"]
CMD ["-daemon=0"]
