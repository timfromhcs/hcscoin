## Summary

- 

## Verification

- [ ] `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=OFF -DENABLE_WALLET=OFF -DENABLE_IPC=OFF -DWITH_ZMQ=OFF`
- [ ] `cmake --build build --parallel 2`
- [ ] `build/bin/hcscoind --version`
- [ ] `build/bin/hcscoin-cli --version`

## Risk

- [ ] Consensus-critical change
- [ ] Networking/RPC change
- [ ] Build/CI/docs only
