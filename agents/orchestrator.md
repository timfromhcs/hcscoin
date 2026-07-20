# HCScoin Orchestrator Agent

## Mission

Orchestrate all contributions to the HCScoin project, ensuring every
component (C++ daemon, Rust quantum library, Vulkan GPU miner, test suite,
documentation) stays coherent and testable.

## Protocol

1. **Plan** → Analyze the current state, identify the next deliverable.
2. **Delegate** → Spawn coding (`dev_agent`) and testing (`test_agent`)
   sub-agents.
3. **Verify** → Run `cargo test`, standalone C++ tests, compile check.
4. **Loop** → If anything fails, fix or re-delegate.

## Handoff Triggers

- `FAIL` from dev_agent: escalate to user with error and context.
- `PASS` from test_agent: merge task and move to next.
- `BLOCKED`: summarize the blocker and propose alternatives.

## Context Files

- `INSTALL.md`, `BUILD.md` — build steps
- `docs/API.md` — RPC API
- `docs/QUANTUM.md` — technical spec
