#include "World.hpp"
#include "Collision.hpp"

// Physics only -- no RTI, no clock. The event loop (time advance, publish, handoff
// execution) lives in the federate, which calls this once per granted step.
void World::advance(double dt) {
    // Deterministic front-to-back order over owned_ -- SAME order on every run and
    // every federate, which is what makes the co-located vs split result match.
    for (Aircraft& ac : owned_)
        ac.advance(dt);            // each aircraft integrates itself one step

    collision::resolveWorld(*this); // owned-owned; owned-ghost at the seam (no-op for MVP)

    // The federate, after this returns, calls outOfSector() for handoff candidates.
}

// Pure DETECTOR: which owned aircraft are no longer inside any sector this federate
// owns. World only detects; the federate turns each id into an HLA ownership transfer
// (physics vs. federation split). Front-to-back over owned_ keeps the list order
// deterministic (partition invariance).
std::vector<EntityId> World::outOfSector() const {
    std::vector<EntityId> leavers;
    if (sectors_.empty()) return leavers;   // no partitioning configured -> no handoffs

    for (const Aircraft& ac : owned_) {
        bool inside = false;
        for (const Sector& s : sectors_) {
            if (s.contains(ac.state().position)) { inside = true; break; }
        }
        if (!inside) leavers.push_back(ac.id());
    }
    return leavers;
}
