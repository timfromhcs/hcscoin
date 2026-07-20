// HCScoin: Common definitions for Vulkan GPU shaders.
// Shared buffer layouts and constants.

#ifndef HCSCOIN_COMMON_GLSL
#define HCSCOIN_COMMON_GLSL

// Statevector: array of complex f32 pairs
struct C32 {
    float re;
    float im;
};

// Single-qubit gate parameters
struct GateParams {
    uint gate_type;  // 0=H, 1=X, 2=Z, 3=S, 4=T
    uint wire;
    uint control;
    uint target;
    uint padding;
};

// Global constants matching statevector.rs
const float FRAC_1_SQRT_2 = 0.70710677;
const uint MAX_QUBITS = 30;
const uint MAX_QUBIT_STATE_SIZE = 1u << 27; // 134M for 27 qubits

#endif
