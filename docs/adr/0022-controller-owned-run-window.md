# ADR-0022: The Controller Owns the Run Window; the Constructive Core Needs No HLA Time Management

- **Status:** Accepted
- **Date:** 2026-09-13
- **Deciders:** Kent

## Context

The aircraft federate originally ran a hardcoded `STEPS = 100` loop, then idled. Two problems:
the run length was an arbitrary constant buried in the federate, and "finish my 100 then idle"
is the wrong shape once aircraft migrate — a federate that has finished its own aircraft may
still need to *adopt* one handed to it late and carry it to the end.

More broadly: what coordinates *time* across a peer HLA federation? The idiomatic answer is HLA
**time management** — regulating/constrained federates advancing under the RTI with a lookahead,
so no federate ever receives an event in its logical past. We deliberately have not built that
(the fedamb's `timeAdvanceGrant` is stubbed, `isRegulating = false`). The question is whether the
constructive core *needs* it, and if not, what owns time instead.

## Decision

1. **The controller owns the run window and disseminates it.** `StartRun` carries `NumSteps`
   (a controller-side constant for now); the federate runs exactly that logical window. A "job"
   is a *space × time* assignment (a sector over `[0, NumSteps]`); the controller sets both.
   Time is deliberately **not** re-partitioned mid-run — no time-splitting of a single entity's
   timeline, which would break causality and force optimistic rollback.
2. **The federate serves until Shutdown, not for a fixed count.** Its loop advances every owned
   aircraft toward `NumSteps` one `dt` at a time and keeps serving — ready to adopt a late
   handoff and carry it to the end — until the controller broadcasts `Shutdown` (still a manual
   operator ENTER; no auto-teardown). "There is always more work until the sim is declared
   complete."
3. **No HLA time management in the constructive core.** Because aircraft are **independent**
   (no ghosts, no collision coupling — ADR-0019), there are no cross-federate events, so no
   federate can ever receive anything in its past. Time management exists to coordinate
   *interacting* federates; with nothing to coordinate, a controller-set window plus each
   aircraft's own step counter is sufficient and correct. This is task assignment, not a
   synchronisation layer — so it does not cross the charter's "consume the RTI, don't build a
   sync layer" line.

## Alternatives Considered

- **Keep the hardcoded federate step count.** *Buys:* nothing to send. *Costs:* the run length
  isn't the controller's to set, and a fixed local count drops a late-adopted aircraft's tail —
  a coverage bug at the seam. Rejected.
- **Introduce HLA time management now (regulating/constrained + lookahead).** *Buys:* the
  idiomatic, general mechanism; required later for the Live experiment. *Costs:* real machinery
  (advance/grant, lookahead, deadlock handling) to coordinate events that, for independent
  aircraft, do not exist. Pure overhead against the deadline for the constructive core. Deferred
  to the Live experiment, where interacting ghosts make it necessary.
- **Central job scheduler that also re-partitions time mid-run.** *Buys:* expressive (e.g. split
  one way for the first half, another for the second). *Costs:* splitting a single trajectory
  across federates in time is exactly the straggler/rollback problem (optimistic synchronisation);
  out of scope. Rejected for the MVP.

## Consequences

- The run length lives on the controller and is broadcast; the federate's `runLoop` and the old
  `serveUntilShutdown` merge into one serve loop that ends only on `Shutdown`.
- This is the property that makes peer migration (ADR-0020) robust: nobody stops at a local
  count, so an adopted aircraft is always carried to the shared end as long as it arrives before
  Shutdown.
- Completion is *reported*, not auto-detected: the federate prints an "all current tasks done"
  transition (re-arming on a late adoption) and per-aircraft completion; the operator still ends
  the run by hand. Controller-tracked completion (all `(id, step ≤ end)` accounted for → auto
  Shutdown) is a possible later refinement, noted not built.
- When the Live experiment introduces coupled ghosts, HLA time management returns as a genuine
  need — this ADR scopes it *out of the constructive core*, not out of the project.
