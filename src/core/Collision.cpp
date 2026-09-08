#include "Collision.hpp"
#include "World.hpp"
#include "Aircraft.hpp"

namespace collision {

// Seam only for the MVP -- all three are no-ops until feature #2 / the stretch.

void resolveWorld(World& world) {
    // TODO(feature #2): interior owned-owned pairs via resolveElastic;
    //                   then (stretch) owned-vs-halo-ghost via resolveAgainstGhost.
}

void resolveElastic(Aircraft& a, Aircraft& b) {
    // TODO(feature #2): elastic impulse along the line of centres (uses masses/radii).
}

void resolveAgainstGhost(Aircraft& a, const Ghost& g) {
    // TODO(stretch): deterministic tie-break owner resolves against the halo ghost.
}

}  // namespace collision
