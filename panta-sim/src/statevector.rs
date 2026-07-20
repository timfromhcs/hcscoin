//! Statevector core: exact-f32 complex arithmetic, Clifford+T gate set.
//!
//! Every arithmetic expression here is mirrored in the C++ fallback
//! (`src/consensus/quantum.cpp`) with the same operand order, so both
//! implementations produce bit-identical statevectors.

use crate::MAX_QUBITS;

/// 1/sqrt(2) rounded to nearest f32 (0x3F3504F3). Exact constant shared
/// with the C++ implementation.
pub const FRAC_1_SQRT_2: f32 = 0.70710677f32;

/// Complex number with f32 components (amplitude type).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct C32 {
    pub re: f32,
    pub im: f32,
}

impl C32 {
    pub const ZERO: C32 = C32 { re: 0.0, im: 0.0 };
    pub const ONE: C32 = C32 { re: 1.0, im: 0.0 };

    #[inline]
    pub fn add(self, o: C32) -> C32 { C32 { re: self.re + o.re, im: self.im + o.im } }
    #[inline]
    pub fn sub(self, o: C32) -> C32 { C32 { re: self.re - o.re, im: self.im - o.im } }
    #[inline]
    pub fn neg(self) -> C32 { C32 { re: -self.re, im: -self.im } }
    /// Multiplication by i.
    #[inline]
    pub fn mul_i(self) -> C32 { C32 { re: -self.im, im: self.re } }
    /// Scaling by real constant.
    #[inline]
    pub fn scale(self, k: f32) -> C32 { C32 { re: self.re * k, im: self.im * k } }
    /// |amp|^2 in f64 (identical operation order everywhere).
    #[inline]
    pub fn norm2(self) -> f64 {
        (self.re as f64) * (self.re as f64) + (self.im as f64) * (self.im as f64)
    }
}

/// A dense statevector of `2^nqubits` amplitudes.
pub struct Statevector {
    pub nqubits: u8,
    pub amps: Vec<C32>,
}

impl Statevector {
    /// Allocate the |00...0> state. Errors if qubits > MAX_QUBITS.
    pub fn new(nqubits: u8) -> Result<Self, &'static str> {
        if nqubits > MAX_QUBITS { return Err("too many qubits"); }
        let size = 1usize << nqubits;
        let mut amps = vec![C32::ZERO; size];
        amps[0] = C32::ONE;
        Ok(Statevector { nqubits, amps })
    }

    #[inline]
    fn for_pairs<F: FnMut(&mut C32, &mut C32)>(&mut self, q: u8, mut f: F) {
        let step = 1usize << (q + 1);
        let half = 1usize << q;
        let mut base = 0usize;
        while base < self.amps.len() {
            for off in 0..half {
                let i0 = base + off;
                let i1 = i0 + half;
                let (lo, hi) = self.amps.split_at_mut(i1);
                f(&mut lo[i0], &mut hi[0]);
            }
            base += step;
        }
    }

    /// Hadamard on qubit q.
    pub fn h(&mut self, q: u8) {
        let k = FRAC_1_SQRT_2;
        self.for_pairs(q, |a, b| {
            let s = a.add(*b);
            let d = a.sub(*b);
            *a = s.scale(k);
            *b = d.scale(k);
        });
    }

    /// Pauli-X on qubit q.
    pub fn x(&mut self, q: u8) {
        self.for_pairs(q, |a, b| core::mem::swap(a, b));
    }

    /// Pauli-Z on qubit q.
    pub fn z(&mut self, q: u8) {
        self.for_pairs(q, |_a, b| *b = b.neg());
    }

    /// S (phase) on qubit q.
    pub fn s(&mut self, q: u8) {
        self.for_pairs(q, |_a, b| *b = b.mul_i());
    }

    /// T (pi/8) on qubit q: |1> amplitude multiplied by (k + i*k).
    pub fn t(&mut self, q: u8) {
        let k = FRAC_1_SQRT_2;
        self.for_pairs(q, |_a, b| {
            let re = (b.re - b.im) * k;
            let im = (b.re + b.im) * k;
            *b = C32 { re, im };
        });
    }

    /// CNOT with control c and target t (c != t).
    pub fn cnot(&mut self, c: u8, t: u8) {
        let cmask = 1usize << c;
        let tmask = 1usize << t;
        for i in 0..self.amps.len() {
            if (i & cmask) != 0 && (i & tmask) == 0 {
                let j = i | tmask;
                self.amps.swap(i, j);
            }
        }
    }

    /// Index of the amplitude with maximum |amp|^2 (deterministic
    /// "measurement": the dominant computational basis state).
    pub fn argmax_measurement(&self) -> u64 {
        let mut best = 0u64;
        let mut bestp = -1.0f64;
        for (i, a) in self.amps.iter().enumerate() {
            let p = a.norm2();
            if p > bestp {
                bestp = p;
                best = i as u64;
            }
        }
        best
    }

    /// Total probability (should stay ~1.0; used by tests).
    pub fn total_prob(&self) -> f64 {
        let mut acc = 0.0f64;
        for a in &self.amps {
            acc += a.norm2();
        }
        acc
    }

    /// Serialize `count` amplitudes sampled at a fixed stride as LE bytes
    /// (input to the state fingerprint hash). Stride = size / count.
    pub fn sampled_bytes(&self, count: usize) -> Vec<u8> {
        let count = count.min(self.amps.len());
        let stride = self.amps.len() / count;
        let mut out = Vec::with_capacity(count * 8);
        let mut i = 0usize;
        for _ in 0..count {
            out.extend_from_slice(&self.amps[i].re.to_le_bytes());
            out.extend_from_slice(&self.amps[i].im.to_le_bytes());
            i += stride;
        }
        out
    }
}


#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn h_creates_superposition() {
        let mut sv = Statevector::new(3).unwrap();
        sv.h(0);
        assert!((sv.amps[0].re - FRAC_1_SQRT_2).abs() < 1e-6);
        assert!((sv.amps[1].re - FRAC_1_SQRT_2).abs() < 1e-6);
        assert!((sv.total_prob() - 1.0).abs() < 1e-5);
    }

    #[test]
    fn cnot_entangles() {
        let mut sv = Statevector::new(2).unwrap();
        sv.h(0);
        sv.cnot(0, 1);
        // Bell state: amps at |00> and |11>
        assert!((sv.amps[0].norm2() - 0.5).abs() < 1e-5);
        assert!((sv.amps[3].norm2() - 0.5).abs() < 1e-5);
        assert!(sv.amps[1].norm2() < 1e-10);
        assert!(sv.amps[2].norm2() < 1e-10);
    }

    #[test]
    fn t_gate_exactness() {
        let mut sv = Statevector::new(1).unwrap();
        sv.x(0);
        sv.t(0);
        assert!((sv.amps[1].re - FRAC_1_SQRT_2).abs() < 1e-7);
        assert!((sv.amps[1].im - FRAC_1_SQRT_2).abs() < 1e-7);
    }
}
