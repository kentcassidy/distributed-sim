# ADR-0016: Core Object Model — Composition-First, Shallow Interfaces

- **Status:** Accepted
- **Date:** 2026-09-03
- **Deciders:** Kent

## Context

`dff_core` needs an object model — aircraft, dynamics, the world/sector — and it
must connect to the RTI-facing federate **without dragging Portico into the
core** (ADR-0014's rule, and the reason the C++17 / gnu++14 split works at all).
The domain was described by a reviewer as "inheritance-heavy," which raises the
real question: how deep an inheritance tree should the model have?

Deep *is-a* taxonomies (`WorldObject → Vehicle → Aircraft → F16`) are brittle —
every cross-cutting change fights the hierarchy, and archetypes become subclasses
instead of data. The frameworks actually used in this community tell a different
story: AFSIM models a platform as *components it has* (mover, sensors,
processors) with shallow polymorphic interfaces; game/ECS designs push the same
idea further (data + systems, not class trees). "Inheritance-heavy" in practice
means *many small interfaces*, not a tall entity tree. This decision fixes the
shape of every core header and the core↔federate relationship.

## Decision

1. **Composition over inheritance for identity.** An `Aircraft` *has-a* `State`,
   `AircraftParams`, and a `DynamicsModel`; it is **not** a subclass of a generic
   entity. Archetypes (F-16 vs Cessna) differ by *data* and *which
   `DynamicsModel` they carry* — config-driven (ADR-0013) — not by subclassing.
2. **Inheritance only for interchangeable behaviour behind an interface.** The
   justified hierarchies are `DynamicsModel` (abstract) → `LinearLongitudinal`;
   optionally `Integrator` (abstract) → `RK4`; and the RTI-mandated
   `FederateAmbassador` → `NullFederateAmbassador` (which lives in the *federate*
   layer, not the core). Each is **one level deep** and exists to swap behaviour
   at runtime.
3. **No premature `Entity` base.** A common base is added only when a *second*
   entity type exists (missiles, ground objects, …). The MVP is all-aircraft, so
   `std::vector<Aircraft>` needs no base class (YAGNI).
4. **Ownership discipline: exactly one owner; everyone else borrows.** The
   `World` owns its aircraft **by value** (`std::vector<Aircraft>`). Relationships
   (a formation leader, a collision pair) are expressed with **non-owning**
   references — raw pointer, index, or id — never a second owner. `unique_ptr`
   appears only where polymorphism or address stability actually demands it.
5. **Core↔federate is composition, not inheritance.** The federate *has-a*
   `World` and calls `world_.advance(dt)`; it does not inherit `World`. This call
   *is* the RTI/physics boundary:
   - **`dff_core`** — `World::advance(dt)` = integrate the owned entities + resolve
     collisions. **No RTI calls.**
   - **`aircraft_federate`** — the event loop (`join`, `requestTimeAdvance`/grant,
     `updateAttributeValues`, handoff, `resign`) wraps the core and calls into it.
6. **Distributed truth model.** There is **no shared memory**. The federate that
   owns an entity's sector is the sole authority that advances it; its result is
   the truth other federates adopt as ghosts. Partition invariance is the test
   that a *re-hosted* computation reproduces that truth — a mismatch is a defect,
   by design.

## Alternatives Considered

- **Composition-first, shallow interfaces (chosen).** *Buys:* matches
  config-driven placement (ADR-0013/0015); keeps the core clean, Portico-free,
  and unit-testable; puts polymorphism where it does honest work (models, the
  ambassador). *Costs:* a little interface design up front.
- **Deep entity taxonomy.** *Buys:* superficially tidy. *Costs:* brittle;
  archetypes become subclasses instead of config; cross-cutting change is
  painful. Rejected.
- **Full ECS in the core.** *Buys:* maximal decoupling, cache-friendly at scale.
  *Costs:* overkill for MVP N, and it obscures the physics for a reviewer who
  wants to read the model. Kept as inspiration, not adopted.

## Consequences

- `dff_core` public headers stay **C++14-safe** (ADR-0014) and Portico-free; the
  federate *composes* the core rather than inheriting it.
- The demonstrable inheritance for interviews is the **`DynamicsModel` hierarchy**
  and the **unavoidable `FederateAmbassador` override** — not an entity tree. That
  is the honest answer to "inheritance-heavy."
- There are two entity collections at the federate boundary: **owned**
  (authoritative, integrated) and **ghosts** (reflected, read-only). Whether the
  ghost set lives inside `World` or in the federate is an implementation choice to
  settle when M4 lands.
- Collision resolution is a `World` responsibility over the owned set (plus halo
  ghosts at a seam) and stays RTI-free — consistent with ADR-0015.
