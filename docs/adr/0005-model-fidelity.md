# ADR-0005: Linearized Longitudinal Model as the Fidelity Floor

- **Status:** Proposed
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

The flight model's fidelity sets both how realistic the dynamics are and how
verifiable they are. Higher fidelity looks more impressive but is harder to
check against a known answer. For a flight-qualities audience, verifiability
against a truth source is a stronger signal than raw realism. The choice
constrains week two onward and is costly to raise later.

## Decision

The baseline flight model is the **linearized longitudinal state-space model**:
four states, constant stability derivatives, no coefficient-table interpolation.
Lateral-directional dynamics and tabulated coefficients are horizon items, added
only if the schedule allows.

## Alternatives Considered

- **Linearized longitudinal.** *Buys:* a closed-form verification target — the
  system eigenvalues are the published short-period and phugoid modes — and a
  clean, cheap likelihood for the later MCMC inversion. *Costs:* motion is
  restricted to the longitudinal plane; no turning formation without the
  lateral extension.
- **Tabulated-coefficient 6-DOF.** *Buys:* realism and full lateral-directional
  motion. *Costs:* substantial extra implementation, harder verification, and
  time taken from the distributed-simulation work that is the actual subject.

## Consequences

- Verification is trivial and analytical: computed eigenvalues versus published
  mode characteristics, recorded as the first V&V result.
- The choice is stated in the report as deliberate — fidelity chosen for
  verifiability — which reads as test-engineer reasoning rather than a shortcut.
- Formation geometry is initially constrained to the longitudinal plane
  (e.g. trail), informing ADR-0010.
- Raising fidelity later is additive (extend the state, add tables) and does not
  invalidate the distributed-simulation work built on top.
