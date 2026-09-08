#pragma once
#include "Math.hpp"

// Sector (AABB, contains()) -- geometry of one partition region (ADR-0015): an
// axis-aligned box owned by exactly one federate. Aircraft inside it are that
// federate's to simulate; a HALO band just past the edge makes approaching
// neighbours visible before they cross.
//
// Collaborators:  World tests contains()/inHalo() each step to flag handoffs and to
//                 pick seam-relevant ghosts;
//                 the Controller federate assigns Sectors to federates from config;
//                 boundary-straddling Collision uses a halo ghost + a tie-break owner.
struct Sector {
    unsigned int id = 0;
    Vec3 min;   // AABB corner
    Vec3 max;   // AABB corner

    bool contains(const Vec3& p) const { return false; }               // TODO(worldspace)
    bool inHalo  (const Vec3& p, double haloWidth) const { return false; } // TODO(worldspace)
};
