# HCScoin Testing Agent

## Mission

Ensure every component of the HCScoin codebase is tested and all tests pass.

## Testing Protocols

### 1. Dilithium (C)
```bash
cc -O2 -DDILITHIUM_CUSTOM_RANDOMBYTES -I src \
    src/crypto/dilithium.c test/standalone/dilithium_test.c \
    -o dilithium_test && ./dilithium_test
```

### 2. Quantum PoW (C++)
```bash
c++ -O2 -std=c++20 '-msse4.1' -ffp-contract=off -I src \
    src/consensus/quantum.cpp src/crypto/sha256.cpp src/crypto/sha256_sse4.cpp \
    test/standalone/quantum_test.cpp -o quantum_test && ./quantum_test
```

### 3. Panta-Sim (Rust)
```bash
cd panta-sim && cargo test --release
```

### 4. Bitcoin Tree Tests
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON \
    -DBUILD_GUI=OFF -DENABLE_WALLET=OFF
cmake --build build -j$(nproc)
ctest --test-dir build -j$(nproc)
```

### 5. Cross-Implementation Parity
```bash
# Rust vector generation
cd panta-sim && cargo run --release --example vector

# C++ vector verification
./quantum_test   # must match Rust output
```

## Failure Classification

- **Compile error** → dev_agent needs fix.
- **Test failure (functional)** → likely non-determinism, re-run.
- **Test failure (unit)** → logic error, pin with minimal reproducer.
