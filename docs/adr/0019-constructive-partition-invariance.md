# ADR-0019: Constructive Bit-Exact Partition Invariance Is the V&V Spine; Live Degradation Is a Separate Experiment

- **Status:** Accepted
- **Date:** 2026-09-09
- **Deciders:** Kent

## Context

The project's validity *claim* and its visually *impressive* result are easy to
conflate, and conflating them has already caused churn. Two different simulations live
under one roof. The charter's "two claims, in order" distinguishes them, but no single
ADR pins the distinction — so an agent (or a reviewer) can mistake the second for the
first: reaching for dead-reckoning machinery while building the core, or expecting an
error threshold where none is allowed.

## Decision

The MVP's V&V spine is **constructive partition invariance, targeted at bit-exact**.
Each federate computes the **exact truth** for the aircraft it owns, every step, and
reports that truth to the controller over the RTI. For a fixed seeded scenario the
**assembled truth must be identical no matter how the work is partitioned** across
federates (K=1 co-located vs K=2 split, and generally N-over-K). The target is **zero
variation** — bit-exact on one host / same image; only across heterogeneous hosts does
floating-point ordering soften the honest claim to tight-tolerance (ADR-0013). This
path uses **no ghosts and no dead reckoning**; there is no accepted error.

**Dead reckoning, ghosts, and injected latency belong to a separate, later
experiment — the "Live" simulation** — which deliberately trades exactness for
tolerated error and measures graceful degradation vs. delay (the charter's "controlled
breakdown"). Ghosts are load-bearing there, and only there.

## Alternatives Considered

- **Make the degradation study the headline (ghosts / DR / latency first).** *Buys:* a
  dynamic "live" demo early. *Costs:* it presupposes and obscures the constructive
  result — without first proving distribution is transparent at zero impairment, a
  measured degradation has no baseline to degrade *from*. Rejected as the primary claim.
- **Tolerance-only invariance (accept a threshold from the start).** *Buys:* simpler;
  no strict determinism needed. *Costs:* concedes the strongest claim — that
  distributing the work changed *nothing* — and blurs the line with the Live
  experiment. Rejected for the core.
- **Constructive bit-exact core, Live degradation second (chosen).** *Buys:* the
  strongest, cleanest validation claim, and a clear staged path to the degradation
  result on top of it. *Costs:* the core demands strict determinism plumbing
  (conservative time, fixed dt / order / seed, no RNG in truth) — accepted, and worth it.

## Consequences

- Determinism is a hard requirement of the truth path: fixed `dt`, fixed iteration
  order, conservative time management, same seed, turbulence off, no RNG. Any
  nondeterminism in truth is a bug (ties to ADR-0005's filler-but-deterministic physics).
- The controller is an aggregator/comparator, not a physics engine; truth flows to it
  (streamed, or flushed per sector — a delivery choice, open pending whether aircraft
  are coupled during the run).
- Ghosts / dead reckoning (ADR-0007's coupling under impairment, ADR-0008's latency)
  are explicitly scoped **out** of the constructive result and **into** the Live
  experiment.
- Reinforces ADR-0011 (constructive scope) and ADR-0004 (validation framing); the
  charter's "V&V Spine: Partition Invariance" section is the narrative form of this
  record.
