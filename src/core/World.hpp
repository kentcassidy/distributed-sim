#pragma once
#include <vector>
#include "Aircraft.hpp"
#include "Sector.hpp"
#include "State.hpp"

// Ghost -- a read-only copy of a NEIGHBOUR aircraft (owned by another federate),
// corrected on reflect and dead-reckoned in between. Carries the last-update logical
// time so extrapolation -- and its staleness -- is explicit (that staleness is the
// variable the boundary-collision V&V probes; see Collision.hpp).
struct Ghost {
    EntityId id = 0;
    State    state;
    double   lastUpdateTime = 0.0;   // logical time of the last reflect
};

// World (owns vector<Aircraft>, advance(dt)) -- the RTI-free physics container for
// ONE federate's slice. Steps all owned aircraft + resolves collisions. No RTI.
//
// Ownership:  owns owned_ (BY VALUE) and sectors_; ghosts_ are copies fed in from the
//             federate's reflect callbacks (read-only neighbour state).
// Collaborators:  the federate HAS-A World and calls advance(dt) each granted step,
//                 then reads owned() to publish and (later) outOfSector() to drive
//                 HANDOFF -- World DETECTS a crossing, the federate EXECUTES the HLA
//                 ownership transfer;
//                 advance() calls collision::resolveWorld();
//                 Sector + Ghost feed the halo/seam logic.
class World {
public:
    void advance(double dt);   // integrate owned_, then resolve collisions

    // access for the federate (publish / handoff / ghost feed)
    std::vector<Aircraft>&       owned()         { return owned_; }
    const std::vector<Aircraft>& owned()   const { return owned_; }
    std::vector<Ghost>&          ghosts()        { return ghosts_; }
    const std::vector<Sector>&   sectors() const { return sectors_; }

    // TODO(worldspace): outOfSector() -> owned aircraft that left their sector
    // (handoff candidates); haloGhosts() -> ghosts inside a sector's halo band.

private:
    std::vector<Aircraft> owned_;    // this federate's authoritative aircraft
    std::vector<Ghost>    ghosts_;   // read-only neighbours (fed by reflect)
    std::vector<Sector>   sectors_;  // regions this federate owns
};
