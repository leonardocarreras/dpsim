---
title: "EMT to SFA Conversion Operators"
linkTitle: "Conversion Operators"
description: >
  The demodulation and remodulation operators that move a boundary quantity between an instantaneous three-phase representation and a complex envelope.
weight: 9
---

`CPS::Signal::SFAConversionOperators` converts a boundary quantity between an instantaneous three-phase representation and a complex envelope.
It is the shared implementation for any component that couples an `EMT::Ph3` subsystem to a shifted-frequency or dynamic-phasor subsystem, so that two such components cannot drift apart in their conventions.
The class is pure algebra: it holds no state, no history and no filter, and it introduces no delay.

## Scale

Every operator is scale-free.
Input to a demodulator is instantaneous three-phase in EMT peak-phase units, which is what `EMT::Ph3` node voltages and component currents already are.
The output is a complex envelope in peak-phase amplitude, so a balanced set of peak-phase amplitude `A` demodulates to an envelope of magnitude `A` with no residual factor.
`scaleIdentityResidual` returns the deviation from that identity and exists so a caller can assert it at construction.

The caller, not the operator, applies the domain scale factor.
`DP::Ph1` is RMS line-to-line, so a voltage envelope leaving the demodulator is multiplied by `PEAK1PH_TO_RMS3PH` and a current envelope by `RMS3PH_TO_PEAK1PH`, with the reciprocals on the way back into `remodulate`.
Use the constants from `Definitions.h` by name rather than re-deriving them.
Apply both factors or neither: they are reciprocal and cancel in any power-preserving bilinear form, so a one-sided application is invisible in every matrix-level diagnostic and shows up only in a physical residual.

The operators stay scale-free because the same demodulation runs on a voltage and on a current, and those two take reciprocal factors.
An operator that carried a scale would need to be told which quantity it holds, which is a parameter that exists only to select a constant.

## Carrier phase

The carrier phase is `fmod(omega0 * t, 2 * pi)` evaluated on the solver's own simulation time.
It must never be accumulated from a step counter and never derived from a sample index.
A timebase error of 0.1 percent produces an envelope error of `0.126` in the counter form against `5.9e-16` in the solver-time form, and that error grows linearly with run length, so a short test will not catch it.

An envelope held across a macro step therefore remodulates against a moving carrier.
That is the intended behaviour, and it is what keeps the hold from injecting a carrier-phase error.

## Sequence convention

With `a = exp(j * 2 * pi / 3)`, the positive sequence is `P = (E_a + a E_b + a^2 E_c) / 3` and the inverse is `E_a = P`, `E_b = a^2 P`, `E_c = a P` for a purely positive-sequence set.
The space vector uses the amplitude-invariant normalisation `s = (2/3) (v_a + a v_b + a^2 v_c)`, which is what makes the scale identity hold with no trailing constant.

`positiveSequenceToPhases` and `phasesToPositiveSequence` are the adapters between a single-phase envelope side and a three-phase side.
They are separate entry points because `positiveSequenceToPhases` is where a `Ph1` boundary's inability to carry negative and zero sequence is localised, so it is the one place to assert balance.

## Arms

The demodulation arm is a constructor argument, not a separate class.

`Arm::SpaceVector` is the default and is exactly optimal on a balanced three-wire boundary.
It is algebraic, delay-free and blind to zero sequence by construction.
Under unbalance its envelope carries a component at twice the fundamental, which is the negative sequence in the conjugate image rather than an error; whether that is acceptable is governed by the macro step and is the caller's decision.

`Arm::AnalyticCompanion` takes the quadrature quantity from a companion run at the same instant and returns exact per-phase envelopes, delay-free, including the negative and zero sequence.
If the companion quantity is unavailable this arm is unavailable; there is no degraded mode.
Each arm accepts only its own demodulator, so a missing companion cannot silently fall back to the space vector.

`demodulateWindowedDft` is a sliding one-cycle DFT kept as a measured baseline outside the interface path.
Its exact nulls at every multiple of the fundamental make it a perfect unbalance and harmonic rejector, and therefore unable to transport either, at a group delay of half a fundamental period.
The nulls require the one-cycle window to be an integer number of time steps, so obtain the window from `integerWindowSamples`, which throws rather than rounding silently.
Rounding is what costs the nulls their depth: 60 Hz on a 50 microsecond grid is 333.33 steps, and truncating it collapses the rejection by six orders of magnitude.

## What a caller must assert

Assert the scale identity at construction, since it is the one guard against a mismatch that reads as a conversion bug and is a unit convention.
Assert that both scale factors are applied, in one place per boundary, for the reason given above.
On an unbalanced boundary using the space vector, assert that the macro step is small enough to carry twice the fundamental, or record that the negative sequence is being discarded.
That last failure is silent: the run stays stable and the boundary simply becomes balanced whether the network is or not.
