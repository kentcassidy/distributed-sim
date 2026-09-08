#pragma once

class Aircraft;   // fwd
class World;      // fwd
struct Ghost;     // fwd

// Collision -- elastic-resolution helpers (pairwise bounce; impulse along the line
// of centres). Called by World. NO-OP for the MVP: World::advance already calls
// resolveWorld() so the seam exists now, and collision becomes an implementation
// later rather than a refactor.
//
// STAGING (formation-then-collision decision; ADR-0015):
//   Tier 1 (MVP)        : none.
//   Tier 2 (feature #2) : INTERIOR collision -- both aircraft in one sector, so one
//                         federate owns both states and computes one bounce. Local, easy.
//   Tier 3 (stretch)    : BOUNDARY-STRADDLING collision -- aircraft in adjacent sectors
//                         meeting at the seam. Needs the halo GHOST of the neighbour and
//                         a DETERMINISTIC TIE-BREAK OWNER (e.g. lower sector id) so both
//                         sides adopt one identical bounce instead of computing two.
//
// V&V HYPOTHESIS (boundary collision as a probe -- candidate experiment note):
//   Under conservative (logical) time the RESULT is invariant to wall-clock latency,
//   so the network's cost must enter as GHOST STALENESS: the tie-break owner resolves
//   the bounce off a neighbour ghost extrapolated across delay Dt. Predicted failure
//   as Dt (or closing speed) rises -- contact detected a step late or missed entirely,
//   so the aircraft CLIP THROUGH each other (tunneling / lost continuous-collision
//   detection). This is the discrete, visually-obvious twin of formation-keeping's
//   continuous drift; both probe one variable: ghost staleness = DR threshold x delay.
//
// Collaborators:  World::advance calls resolveWorld() after integrating;
//                 reads/writes Aircraft::state() and reads params() (mass, radius);
//                 Tier 3 reads a neighbour Ghost from the World's halo set.
namespace collision {
    void resolveWorld(World& world);                        // owned-owned + owned-ghost seam
    void resolveElastic(Aircraft& a, Aircraft& b);          // interior pair (Tier 2)
    void resolveAgainstGhost(Aircraft& a, const Ghost& g);  // seam pair    (Tier 3)
}
