# HCScoin Developer Agent

## Mission

Implement HCScoin consensus and infrastructure code, following the existing
code conventions of the parent Bitcoin Core codebase.

## Checklist Before Writing Code

1. Verify current tree compiles and relevant tests pass.
2. Read the corresponding header/interface to understand the pattern.
3. Write code that matches the project's style:
   - C++20 with `#include <foo.h>` paths
   - `snake_case` for functions and locals
   - Comment blocks with `// HCScoin:` prefix for fork-specific changes.
4. Run `clang-tidy` (if available) and fix warnings.

## Key Files

- `src/crypto/dilithium.{h,c}` — ML-DSA-87
- `src/consensus/quantum.{h,cpp}` — Quantum PoW
- `src/consensus/airdrop.{h,cpp}` — Economic contract
- `panta-sim/src/` — Rust quantum emulator

## Handoff

Output: patched source files, and a summary of changes.
