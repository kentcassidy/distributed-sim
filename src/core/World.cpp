#include "World.hpp"
#include "Collision.hpp"

// Physics only -- no RTI, no clock. The event loop (time advance, publish, handoff
// execution) lives in the federate, which calls this once per granted step.
void World::advance(double dt) {
    for (Aircraft& ac : owned_)
        ac.advance(dt);            // integrate each owned aircraft (physics TODO in the model)

    collision::resolveWorld(*this); // owned-owned; owned-ghost at the seam (no-op for MVP)

    // TODO(worldspace): flag owned aircraft that left their sector so the federate
    // can hand them off (ownership transfer, or delete-here / create-there).
}
