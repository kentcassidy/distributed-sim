# ADR-0013: Config-Driven Entity/Partition Model — N Aircraft over K Containers

- **Status:** Accepted
- **Date:** 2026-09-01
- **Deciders:** Kent

## Context

The project's validity claim is **partition invariance**: for a fixed seeded
scenario, the aggregate outcome must not depend on how entities are distributed
across federates. Stated for two aircraft it is only "co-located vs split." The
goal is broader — a simulation flexible to an arbitrary number of aircraft with
per-aircraft parameters, whose load can be split across an arbitrary number of
federates/containers. This shapes the core interface, the run configuration, the
random-number design, and how strong (and how honest) the invariance claim can
be. The decision is foundational, so it is made now rather than retrofitted.

A prior point of confusion is worth settling here: a **stochastic** model (random
forcing) and a **reproducible** run (identical output for a fixed seed) are not
in tension. Randomness comes from a *seeded* generator, so a stochastic model
still replays deterministically. Reproducibility is the property; partition
invariance is the stronger claim built on it.

## Decision

Model entities and their placement as **configuration, not code**:

1. **The entity is the divisible unit.** With strict ownership (each aircraft
   integrated by exactly one federate), a partition is an assignment
   `aircraft → federate → container`. The core owns a *list* of aircraft and is
   agnostic to how many it holds or which federate hosts them.
2. **N aircraft, per-aircraft parameters.** A run config declares the aircraft
   set — each with its own initial state, aero derivatives, and role in a
   **formation graph** (trail chain, leader-star, etc.; each follower holds
   station on the dead-reckoned ghost of its assigned target).
3. **K containers, config-driven placement.** The same config maps entities to
   federates/containers, so K=1 (monolithic) and every split are the same code
   path with different placement. Adding aircraft or containers is a config
   change, not a rewrite.
4. **Per-entity seeded RNG streams.** Each aircraft draws its stochastic forcing
   from its own seeded stream, so its noise sequence is identical regardless of
   which federate hosts it — the property that lets turbulence coexist with
   partition invariance.
5. **Invariance across *every* partitioning** becomes the general claim: the
   federation is a faithful decomposition of the monolith for any placement.

## Alternatives Considered

- **Config-driven, entity-agnostic, per-entity RNG (chosen).** *Buys:* N/K
  generality for free; the strongest form of the invariance claim; the MVP is the
  first rung of a real ladder, not a dead end. *Costs:* per-entity RNG discipline
  and a config schema to design up front.
- **Hard-code two aircraft / two federates.** *Buys:* the fastest possible MVP.
  *Costs:* the generalization becomes a rewrite; the invariance claim stays at its
  weakest form. Rejected — the entity-agnostic core costs little now and nothing
  later.
- **Global RNG with turbulence in the MVP.** *Buys:* stochastic forcing sooner.
  *Costs:* draw order changes with the partition, so runs diverge and invariance
  fails spuriously. Rejected for the MVP; the fix is per-entity streams (above),
  scheduled post-MVP with the storm.

## Consequences

- **Determinism has a scope, stated honestly.** Bit-exact invariance is realistic
  for K containers on **one host** from the same image (same binary, same CPU
  features). Across **heterogeneous hosts**, floating-point ordering (FMA,
  transcendentals, reduction order) makes the defensible claim
  *tight-tolerance*, not bit-exact. This bound is reported, not hidden.
- **MVP scope is unchanged.** The MVP still proves invariance with **two aircraft,
  one host, turbulence off** — the simplest clean instance. N aircraft, K
  containers, and seeded turbulence are the documented generalizations that follow
  by configuration.
- **Time management scales with K.** Conservative time (all ships
  regulating + constrained) still holds, but LBTS coordination and deadlock
  sensitivity grow with the number of federates; lookahead choices matter more as
  K rises.
- Relates to ADR-0006 (model core / driver split) and ADR-0008 (impairment
  injection); supersedes the earlier "two aircraft / four-aircraft stretch"
  framing wherever it appears.
