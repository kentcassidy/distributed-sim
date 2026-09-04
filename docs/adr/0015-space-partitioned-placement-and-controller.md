# ADR-0015: Space-Partitioned Placement with a Controller Federate

- **Status:** Accepted
- **Date:** 2026-09-03
- **Deciders:** Kent

## Context

ADR-0013 fixed placement as `aircraft → federate → container` — an *entity*
partition, each aircraft statically owned by one federate for the life of a run.
Introducing rigid, perfectly-elastic **collisions** between aircraft breaks that
model. A collision is a *pairwise* interaction: it needs both aircraft's full
state at the same instant, resolved by a single authority whose result both sides
then adopt. Under an entity partition, two colliding aircraft owned by different
federates force cross-federate coordination every step they are near contact,
plus an agreement rule so the two owners do not compute different bounces and
drift apart. That is the hard case, and with collisions it recurs for every close
pair rather than being rare.

Partitioning **space** instead removes the common case entirely: each federate
owns a region (a *sector*) and authoritatively simulates whichever aircraft are
inside it, so a collision between two aircraft in the same sector is local — one
federate holds both states and computes one bounce. This is the standard approach
in distributed simulation and in large-scale online worlds, for exactly this
reason.

## Decision

1. **Placement is by region, not by entity.** The world is divided into K
   sectors; each sector is owned by exactly one federate. An aircraft is
   simulated by whichever federate owns the sector it currently occupies.
   Membership is *dynamic* — aircraft migrate between federates as they move.
2. **A dedicated controller federate assigns sectors.** It joins the federation
   as a coordinator. **For the MVP it loads a scenario/partition config file and
   delegates** — computes or reads the sector→federate assignment once at
   startup, publishes it, and lets the aircraft federates adopt their sector and
   run. Dynamic rebalancing (reassigning sectors by live capability while the run
   proceeds) is deliberately deferred; the controller is the seam where it will
   later live.
3. **Ownership is the arbiter; the controller only assigns.** Once a sector has
   exactly one owner, HLA attribute ownership already guarantees a single
   authoritative computation there. Convergence between hosts comes from that
   *singularity of computation*, not from ranking sims. Capability-based priority
   (e.g. registered compute power) governs *who is assigned* a sector — a load
   decision — never *whose physics is correct*.
4. **Boundaries use halo ghosts plus handoff.** Each federate subscribes to a
   thin **halo band** just past its sector edge, so approaching traffic is
   visible before it crosses. An aircraft leaving a sector is **handed off**
   (HLA ownership transfer, or delete-here / create-there). A collision
   straddling a seam is resolved by a deterministic tie-break owner using its
   halo ghost of the neighbour.
5. **Manual interest filtering, not DDM, for the MVP.** HLA's Data Distribution
   Management is the standard region-based filter, but Portico 2.1.0's DDM
   support is partial; at MVP scale each federate simply ignores anything outside
   its sector + halo. DDM stays a post-MVP option, not a dependency.

## Alternatives Considered

- **Space partition + controller (chosen).** *Buys:* collisions are local in the
  common case; partition invariance now directly validates the distribution
  machinery, because the boundary/handoff logic is exactly what invariance tests.
  *Costs:* dynamic membership, handoff, and halo bands to build; a
  boundary-straddling collision is the one hard residual case.
- **Keep the entity partition (ADR-0013 unchanged).** *Buys:* static ownership,
  no migration. *Costs:* every cross-federate close pair needs per-step
  coordination and an agreement rule; collision makes that the norm, not the
  exception. Rejected once collision entered scope.
- **Lean on HLA DDM for interest management now.** *Buys:* the standard
  mechanism. *Costs:* Portico 2.1.0 DDM is partial and unproven here; a risk
  against the 9/13 deadline for no benefit at MVP scale. Deferred.

## Consequences

- **Amends ADR-0013.** The divisible unit stays the entity and placement stays
  config-driven, but the *assigned* unit becomes the sector, and an aircraft's
  owner is now a function of position and time rather than a static config line.
  Per-entity seeded RNG streams (ADR-0013 §4) are unchanged and are what keep a
  migrating aircraft's stochastic forcing identical across a change of owner.
- **Partition invariance sharpens.** The claim becomes: a co-located run
  (K=1, one sector spanning the world) and a sector-split run (K>1) produce the
  same trajectories and the same bounces at zero impairment. The handoff and halo
  path is the thing most able to perturb the physics, so invariance is precisely
  its test.
- **A new component enters the architecture:** the controller federate
  (`src/controller/`), plus a sector/halo model in `dff_core` and a migration
  path in the aircraft federate.
- **Collision is the motivating interaction.** Rigid, perfectly-elastic contact
  (impulse along the line of centres) is deterministic and reproducible — good
  for invariance — but a sharp discrete event is the hardest case for cross-host
  floating-point agreement: contact detected one step apart on two hosts diverges
  immediately. Bit-exact single-host stays the MVP floor; cross-host stays
  tight-tolerance (consistent with ADR-0013's determinism scope). Whether
  collision **supplants or joins** formation-keeping (ADR-0007) as the MVP
  coupling scenario is a scope decision to be recorded separately.
- **MVP scope rises to a genuine distributed demonstration:** two aircraft, the
  world split into two sectors on one host, invariance proven across the cut, an
  interior collision resolved locally. Boundary-straddling collision fidelity is
  the stretch that may slip past 9/13 without endangering the demo.
