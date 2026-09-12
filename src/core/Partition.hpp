#pragma once
#include <map>
#include <vector>
#include "AircraftParams.hpp"   // EntityId
#include "Math.hpp"             // Vec3
#include "Scenario.hpp"
#include "Sector.hpp"

// Partition -- the controller's "divide the volume" brain, kept as pure RTI-free
// functions so it is unit-testable with no federation running. The controller reads a
// Scenario, tiles the world volume into K slabs (K = number of aircraft federates that
// joined), and assigns each entity to a slab by its initial position. Same scenario +
// same bounds => same assignment, every run -- which is what the partition-invariance
// claim rests on.

// Axis to slice the volume along. Y for the static MVP: the aircraft fly in +x and are
// offset in y, so a y-split separates them AND none ever crosses a seam (static, no
// handoff). X-split = the later migration case; it is a parameter, not a rewrite.
enum class Axis { X = 0, Y = 1, Z = 2 };

// Tile the world box into k equal slabs along `axis`. Slab ids are 0..k-1 in increasing
// coordinate order (deterministic). Each slab spans the full world extent on the other
// two axes. Returned .min/.max are geometry only (for handing to federates / drawing);
// OWNERSHIP is decided by slabOf(), never by Sector::contains() -- contains() is
// inclusive on both faces and would double-count a seam.
std::vector<Sector> tileVolume(const Vec3& worldMin, const Vec3& worldMax,
                               Axis axis, unsigned int k);

// Which slab (0..k-1) owns point p? UNIFORM half-open rule on every axis:
//     p is in the world  iff  worldMin[d] <= p[d] < worldMax[d]  for every axis d
// -- min face inclusive, max face EXCLUSIVE, no special-cased "last" slab. A point on an
// interior seam falls to the UPPER slab; a point on an outer max face is simply OUTSIDE
// (returns -1). Pure arithmetic (floor), so the answer never depends on k's parity or on
// which slab is last -- exactly what makes it safe to rescale k dynamically later.
int slabOf(const Vec3& worldMin, const Vec3& worldMax, Axis axis, unsigned int k,
           const Vec3& p);

// Assign every scenario entity to a slab index by its initial position. Returns
// entityId -> slab index (std::map => iteration is id-sorted = deterministic). Throws
// std::runtime_error if any entity starts outside the world (including exactly on an
// outer max face) -- a loud config error, never a silently unowned aircraft.
std::map<EntityId, int> assignEntities(const Scenario& scn,
                                       const Vec3& worldMin, const Vec3& worldMax,
                                       Axis axis, unsigned int k);
