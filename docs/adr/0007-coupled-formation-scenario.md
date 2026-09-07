# ADR-0007: Couple the Aircraft via Formation-Keeping

- **Status:** Accepted (locked 2026-09-05: formation-keeping is the MVP coupling; collision is the immediate post-MVP feature)
- **Date:** 2026-08-31
- **Deciders:** Kent

## Context

The experiment needs a dependent variable that responds to network behaviour.
Two aircraft that fly independently and merely exchange position produce nothing
measurable: ghost error affects only what would be drawn on a screen, and
measuring latency in isolation is what `ping` is for. Some coupling between the
aircraft is required for the network to enter the physics.

## Decision

The second aircraft runs a **formation-keeping control law whose reference input
is the dead-reckoned ghost of the first aircraft** — never the first aircraft's
truth. Network delay and dead-reckoning error thus enter a closed physical loop
and emerge as station-keeping error, control activity, and eventually loss of
stability.

## Alternatives Considered

- **Independent aircraft, position exchange only.** *Buys:* simplicity. *Costs:*
  no measurable coupling; no experiment. Rejected.
- **Split one aircraft's plant and controller across two processes.** *Buys:*
  a closed loop across the network. *Costs:* architecturally artificial — no
  real federation is built that way — and it wastes the two-owned-aircraft
  structure. Rejected in favour of formation, which achieves the same coupling
  inside a natural topology.
- **Formation-keeping on the ghost.** *Buys:* a real, physical dependent
  variable, inside the standard entity-ownership topology. *Costs:* requires a
  tracking control law, adding modest implementation.

## Consequences

- The latency-stability experiment has a genuine physical observable.
- The follower consuming the *extrapolated ghost* is what converts network
  degradation into measurable error, tying ADR-0006's architecture to the
  result.
- Formation geometry becomes a decision in its own right (ADR-0010), because it
  determines which axes the tracking law exercises and interacts with the
  longitudinal-only fidelity floor (ADR-0005).
