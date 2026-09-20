# ADR-0020: Peer-to-Peer Ownership Handoff via a Dedicated Interaction

- **Status:** Accepted
- **Date:** 2026-09-13
- **Deciders:** Kent

## Context

ADR-0015 made ownership a function of *position and time* — aircraft migrate between
federates as they move — but left the migration *mechanism* open ("HLA ownership transfer,
or delete-here / create-there"). The migration plan drafted for this increment routed a
departure through the controller: a federate that detects an aircraft has left its sector
sends the controller an "out-of-bounds notice," and the controller decides the new owner
and reassigns.

Working through it with the operator surfaced a cleaner model. In HLA the runtime coordinator
of a peer federation is the RTI, not a central authority; once the controller has handed out
the initial jobs it has nothing left to decide, because the partition is **static and
globally known** (every federate can be told the whole map). Keeping the controller in the
per-crossing loop adds a round-trip and a single point of coordination for a decision every
federate can already make identically.

Determinism also has to hold across the handoff: the handed-off trajectory must be **bit-exact**
versus a never-handed-off run, with every logical step computed exactly once, and with no
assumption that the two federates' wall-clock loops stay in lockstep (we run no HLA time
management — ADR-0022).

## Decision

**Migration is peer-to-peer. After bootstrap the controller leaves the runtime loop.**

1. **The controller broadcasts the full partition map.** Every `AssignSector` (one per cell,
   tagged with its owner) reaches every federate; each federate keeps the whole map. So any
   federate can name the owner of any point on its own.
2. **The owner computes the destination and hands off directly.** When an owned aircraft
   leaves the federate's sector, that federate computes the destination with the same
   half-open rule the controller used (`ownerOf` ≡ `cellOf`), and sends the aircraft's exact
   state straight to the destination federate. If the point is outside every cell (it left the
   world) the owner logs it **lost in the void** and drops it — no reassignment. We rejected a
   broadcast-and-volunteer auction: with a static, globally-known map every federate computes
   the *same single* destination, so an auction is pure chatter with tie-breaks to invent.
3. **A dedicated `Handoff` interaction (fed → fed), separate from `AssignEntity`.** Same
   payload plus a `Step`, but its own class: `AssignEntity` = ownership **created** centrally
   at bootstrap; `Handoff` = ownership **moved** between peers at runtime. The controller
   neither publishes nor subscribes `Handoff`, so the wire contract itself proves it is out
   of the migration loop.
4. **Continuous per-aircraft stepping; one NDJSON line per (aircraft, step).** Each aircraft
   carries its own logical-step counter; each serve-loop pass advances every owned aircraft
   one `dt`, logs it at its own step, and hands it off if it left. An adopted aircraft simply
   *continues* its sequence (owner logs `0..s`, adopter logs `s+1..N`). Logging per-aircraft
   at its own step — not a shared frame time — is what makes coverage exactly-once regardless
   of how the federates interleave.

## Alternatives Considered

- **Controller-decides ("out-of-bounds notice," the earlier plan).** *Buys:* a single place
  that reasons about ownership; natural if the map were private or dynamic. *Costs:* a
  per-crossing round-trip and a central coordinator in the hot path, for a decision every
  federate can already make identically. Rejected once the map is static and broadcast.
- **HLA attribute ownership management** (`negotiatedAttributeOwnershipDivestiture` /
  `attributeOwnershipAcquisition`). *Buys:* the idiomatic HLA transfer primitive. *Costs:*
  moves the decision into a peer negotiation and couples us to Portico's ownership-management
  maturity, when our objects are trivial and we specifically want the *owner* to stay
  authoritative and directed. Recorded as the idiomatic path we deliberately did not take.
- **Global step counter + catch-up on adoption (the planned M5).** *Buys:* one shared clock
  to reason about. *Costs:* an adopted aircraft can arrive ahead of or behind the receiver's
  step, and catch-up must re-log intermediate steps or drop them — fragile under skew.
  Replaced by continuous per-aircraft stepping, which has no shared clock to fall out of sync.

## Consequences

- **Refines ADR-0015 §4.** The handoff is a directed peer interaction computed from the shared
  map, not a controller reassignment and not HLA ownership management. The controller's runtime
  role shrinks to bootstrap + holding the federation open until Shutdown.
- **Bit-exactness is preserved and was verified:** `dff_diff` on a crossing run at K=1 vs K=2
  reported HOLDS, 202/202, with entity 1 owned by A in one run and B in the other — same truth,
  different owner.
- **The federate is partition-shape agnostic.** Because destination is a half-open scan of the
  broadcast cell list, the same code handles single-axis slabs, the RCB tiling (ADR-0021), and
  any future partition, and it routes a fast aircraft that skips a cell straight to the cell it
  actually landed in (not a nearest neighbour).
- **Clutter is deferred, not solved.** Every `Handoff`/`AssignEntity` reaches all federates and
  is filtered by `TargetFederate`; fine at MVP K, an N² concern at large K. DDM (routing regions
  = sectors) stays the scaling answer (ADR-0015 §5).
- **Ownership is explicit in the log.** Each frame carries `owner`; departures also emit
  `handoff` / `out_of_bounds` event lines (viewer metadata, ignored by `dff_diff`).
