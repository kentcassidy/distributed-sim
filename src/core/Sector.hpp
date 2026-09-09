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

    // Is point p inside this axis-aligned box? (inclusive on all six faces)
    bool contains(const Vec3& p) const {
        return p.x >= min.x && p.x <= max.x
            && p.y >= min.y && p.y <= max.y
            && p.z >= min.z && p.z <= max.z;
    }

    // Is p in the halo SHELL -- outside the sector but within haloWidth of it?
    // (the band where a neighbour is visible as a ghost before it crosses the seam)
    bool inHalo(const Vec3& p, double haloWidth) const {
        if (contains(p)) return false;                       // inside the sector, not the shell
        return p.x >= min.x - haloWidth && p.x <= max.x + haloWidth
            && p.y >= min.y - haloWidth && p.y <= max.y + haloWidth
            && p.z >= min.z - haloWidth && p.z <= max.z + haloWidth;
    }
};
