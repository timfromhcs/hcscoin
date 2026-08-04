# syntax=docker/dockerfile:1

FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /src

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ccache \
    ca-certificates \
    cmake \
    git \
    libboost-dev \
    libevent-dev \
    libsqlite3-dev \
    ninja-build \
    pkg-config \
    python3 \
  && rm -rf /var/lib/apt/lists/*

COPY . .

RUN cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_GUI=OFF \
      -DENABLE_WALLET=OFF \
      -DENABLE_IPC=OFF \
      -DWITH_ZMQ=OFF \
      -DBUILD_TESTS=OFF \
  && cmake --build build --target bitcoind bitcoin-cli bitcoin --parallel 2 \
  && strip build/bin/hcscoind build/bin/hcscoin-cli build/bin/hcscoin || true

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive \
    HCSCOIN_DATA=/root/.hcscoin

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libevent-2.1-7t64 \
    libsqlite3-0 \
    libstdc++6 \
  && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/bin/hcscoin /usr/local/bin/hcscoin
COPY --from=builder /src/build/bin/hcscoind /usr/local/bin/hcscoind
COPY --from=builder /src/build/bin/hcscoin-cli /usr/local/bin/hcscoin-cli

VOLUME ["/root/.hcscoin"]
EXPOSE 28333 28332 28335 28336 28337 28338 28339 28340 28341

ENTRYPOINT ["hcscoind"]
CMD ["-daemon=0", "-printtoconsole"]
