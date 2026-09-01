# ADR-0010: Rankine Vortex Storm With Dryden Turbulence

- **Status:** Proposed
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

The scenario needs an environmental forcing that (a) perturbs both aircraft in a
spatially structured way and (b) supplies the stochastic element that makes the
Monte Carlo ensemble meaningful. The forcing is a means, not the subject, so it
should be cheap, defensible, and standard rather than elaborate. It also
determines the one coupling point between environment and flight model.

## Decision

Model the storm as a **Rankine vortex** wind field — tangential velocity rising
linearly inside a core radius and falling as 1/r outside, plus an updraft term —
and add **Dryden turbulence** (band-limited white noise through the standard
shaping filters) as the stochastic component. Wind enters the flight model at a
single point: local wind is subtracted from inertial velocity before computing
angle of attack, sideslip, and dynamic pressure.

## Alternatives Considered

- **Rankine vortex + Dryden.** *Buys:* closed-form, two-parameter storm; a
  turbulence model specified in the military handbooks and standard in flight
  simulation; a single clean coupling point. *Costs:* not a physically complete
  storm — but completeness is not the goal.
- **Higher-fidelity CFD-derived or multi-cell wind field.** *Buys:* realism.
  *Costs:* expensive, unvalidatable within scope, and disproportionate to its
  role as mere forcing. Rejected.
- **Unstructured random perturbations.** *Buys:* trivial. *Costs:* no spatial
  structure, so the two aircraft are not coherently affected and the shared
  worldspace does no work. Rejected.

## Consequences

- The storm supplies spatially-structured forcing; Dryden supplies the
  per-run randomness that makes each ensemble member distinct.
- Wind coupling is one line in the aero path, keeping the model core simple.
- The vortex parameters (center, core radius, circulation) are exactly the
  unknowns the later MCMC inversion infers, so this model does double duty.
- Formation geometry (trail vs echelon vs line-abreast) is chosen here too,
  bounded by the longitudinal-only fidelity floor (ADR-0005) until the lateral
  extension exists.
