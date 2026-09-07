# ADR-0011: Constructive Scope Now; Virtual Accommodated by Design

- **Status:** Accepted (locked at M1, 2026-09-07)
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

In DoD M&S terms, both planned execution modes (batch and federated) are
**constructive** — simulated entities, no human in the loop during execution.
A **virtual** mode (real human operating a simulated aircraft) is a plausible
future extension. Building virtual within the four weeks is out of scope, but
several small decisions made now determine whether it is later a third driver or
a rewrite. The interesting content is *why* virtual is not just a config flag.

## Decision

Scope the project to **constructive execution** (batch and federated). Do **not**
build a virtual mode now. Preserve the option cheaply by: keeping the model core
free of any timing assumption (already required by ADR-0006); making control
input an **interface** with implementations for scripted and autopilot sources,
so an external human input is a third implementation; and making **time policy
per-federate configuration** read at join rather than compiled in.

## Alternatives Considered

- **Constructive now, virtual accommodated (this decision).** *Buys:* keeps the
  ensemble's reproducibility (constructive runs are deterministic given a seed)
  and leaves a clean path to virtual. *Costs:* a small amount of interface
  discipline now for a mode not yet built.
- **Build a virtual (joystick) mode in-scope.** *Buys:* a flashy demo. *Costs:*
  human-in-the-loop federates need best-effort, wall-clock-paced time policy,
  which breaks conservative time management and destroys ensemble
  reproducibility; and a half-working joystick demo shows less than a written
  account of the LVC trade-offs. Rejected.
- **Ignore virtual entirely.** *Buys:* nothing to design around. *Costs:*
  forecloses a natural extension and misses the chance to demonstrate LVC
  literacy. Rejected.

## Consequences

- The report includes a short architecture note: the design supports
  constructive execution in batch and federated modes; a virtual mode is
  accommodated by the driver separation and would require **heterogeneous time
  management** (constructive federates regulating + constrained; a virtual
  federate free-running, best-effort, wall-clock paced) with the loss of
  ensemble reproducibility.
- The replay driver (ADR-0006) is structurally a virtual federate minus the
  input device, so building replay also builds the pacing path virtual needs.
- **Live** execution is explicitly not supported by the engine; live entities
  would enter through a DIS/TENA gateway federate — a horizon item, and the one
  that maps most directly onto range operations.
