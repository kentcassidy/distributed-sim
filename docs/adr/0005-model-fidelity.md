# ADR-0005: Linearized Longitudinal Model as Deterministic Filler

- **Status:** Accepted
- **Date:** 2026-09-09
- **Deciders:** Kent

## Context

This record was opened (2026-08-31) as "the linearized longitudinal model as a
fidelity **floor**," on the reasoning that its eigenvalues are the published
short-period and phugoid modes, giving a closed-form verification target. It was
left **Proposed**, to resolve at M2.

At M2 the project's center of gravity is unambiguous: this is a distributed-systems
proof of concept (ADR-0004, ADR-0019). The flight physics exists only to give the
federation something to integrate and distribute; its realism carries none of the
argument. Sourcing published stability derivatives and verifying eigenvalues against
a textbook would spend effort on the one axis the project does not claim, and it is
unnecessary for the load-bearing result — partition invariance is a property of
**determinism, not fidelity** (ADR-0019).

## Decision

We keep the **linearized longitudinal state-space model** as the model *shape*: four
states (`[u, w, q, theta]`), constant stability derivatives, no coefficient-table
interpolation. But the derivatives are **arbitrary-but-stable, made-up coefficients**
(see `src/core/AircraftParams.hpp`), chosen only so the dynamics are bounded and
deterministic. **Fidelity is out of scope**, and the **published-mode eigenvalue
verification is dropped** (backlogged, off the critical path). The model's V&V is
that it is deterministic and stable; correctness of the *system* is established by
partition invariance (ADR-0019), not by the physics matching any real aircraft.

## Alternatives Considered

- **Fidelity floor with published-mode verification (the original proposal).**
  *Buys:* a closed-form physics-verification result. *Costs:* requires real published
  derivatives, spends time on realism that carries none of the argument, and adds an
  external-truth dependency — all for an axis this project does not claim.
- **Deterministic filler, no fidelity claim (chosen).** *Buys:* a minimal, cheap,
  stable, fully-deterministic model that lets the distributed-systems work proceed
  immediately, with nothing to source or tune. *Costs:* the physics is not
  independently verifiable against nature — accepted, because it is not a claim.
- **Tabulated-coefficient 6-DOF.** *Buys:* realism, lateral-directional motion.
  *Costs:* substantial implementation and time taken from the actual subject;
  rejected as before.

## Consequences

- No aerodynamic data needs to be sourced; `AircraftParams` carries invented, stable
  coefficients and is free to change.
- The eigenvalue mode-check is backlogged; if physics verifiability is ever wanted it
  returns as an optional *integrator self-check* (choose eigenvalues, confirm RK4
  reproduces them) — an integrator test, not a fidelity claim.
- Determinism becomes the model's binding requirement: fixed `dt`, fixed iteration
  order, no RNG in the truth path (ADR-0019). This now matters more than any fidelity
  property.
- Raising fidelity later remains additive (extend the state, add tables) and does not
  invalidate the distributed-simulation work built on top.
- Resolves the original "fidelity floor" framing; the charter's Success Criterion on
  published modes and its Experiments V&V "flight model correct" row are
  correspondingly backlogged.
