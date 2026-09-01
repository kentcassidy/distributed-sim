# ADR-0006: Separate the Model Core From Execution Drivers

- **Status:** Proposed
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

The project has two execution needs that pull in opposite directions. Monte
Carlo analysis wants hundreds of fast, independent runs in a single process. The
distributed-simulation demonstration wants coordinated multi-process execution
with time management and network impairment. Standing up hundreds of full HLA
federations for the ensemble would be slow and operationally miserable. This
tension is the central architectural problem, and how it is resolved shapes the
whole codebase.

## Decision

Build the physics as a **model core library with no knowledge of the RTI, no
clock, and no timing assumptions**, exposing an `advance(dt)` step. Layer
separable drivers on top:

- **Batch driver** — single process, no RTI, N seeds across cores; the ensemble.
- **Federated driver** — the same core wrapped as federates, one scenario at a
  time, with time management, ghosts, dead reckoning, and injected latency.
- **Replay driver** — wall-clock-paced playback of recorded runs.

## Alternatives Considered

- **Monolithic federation.** *Buys:* less scaffolding up front. *Costs:* a dead
  end for the ensemble — hundreds of federated runs are impractical — which
  removes the project's headline statistical result.
- **Model core + drivers.** *Buys:* the ensemble, the batch-vs-federated
  distribution comparison, and a clean account of one validated model used in
  multiple execution modes — how real M&S organizations actually operate.
  *Costs:* an upfront interface-design discipline (the core must stay
  timing-agnostic).

## Consequences

- The headline result — comparing the outcome distribution of monolithic and
  federated execution — becomes possible, because both drivers share one core.
- Keeping the core clock-free also keeps a future **virtual (human-in-the-loop)**
  driver reachable without a rewrite (ADR-0011 territory), since pacing lives in
  the driver.
- The replay driver reuses the federated driver's pacing infrastructure.
- Discipline is required: any timing or RTI dependency leaking into the core is
  a defect to be corrected, not accommodated.
