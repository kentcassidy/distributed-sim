#pragma once
#include <map>
#include <vector>
#include "AircraftParams.hpp"   // EntityId
#include "Math.hpp"             // Vec3
#include "Scenario.hpp"
#include "Sector.hpp"

// Partition -- the controller's "divide the volume" brain, kept as pure RTI-free functions so
// it is unit-testable with no federation running. The world volume is tiled into K disjoint
// cuboid cells by RECURSIVE COORDINATE BISECTION (RCB): repeatedly take the largest-volume
// cell and split it at the geometric MIDPOINT of its LONGEST axis.
//
//   * splitting the LONGEST axis drives every cell toward a cubic aspect ratio (compact cells,
//     short seams) instead of long thin slabs;
//   * splitting the LARGEST cell yields the natural unequal ratios when K is not a power of
//     two -- K=3 -> 1:1:2, K=5 -> 1:1:2:2:2, ... (a kd-tree / RCB partition, standard in
//     parallel computing and mesh/N-body codes).
//
// The tiling is a pure function of (worldMin, worldMax, K) -- NOT of the aircraft positions --
// so the same K always yields the same cells, which is what the partition-invariance claim
// rests on. Ownership of a point is decided by half-open containment (cellOf), never by
// Sector::contains() -- contains() is inclusive on both faces and would double-count a seam.

// Tile the world box into k disjoint cells via RCB. Cell ids are 0..k-1 in a deterministic
// order (split in place: the low half keeps the slot, the high half is inserted after it).
// k==0 -> empty. Each split is at the geometric midpoint, low = [min, mid) and high = [mid,
// max) on the split axis, so the cells tile the world with no gap or overlap. Tie-breaks are
// fixed for determinism: largest cell by volume (ties -> lowest index); longest axis (ties
// X -> Y -> Z).
std::vector<Sector> tileVolume(const Vec3& worldMin, const Vec3& worldMax, unsigned int k);

// Which cell (0..cells.size()-1) contains point p? UNIFORM HALF-OPEN rule on every axis:
//     p is in the cell  iff  cell.min[d] <= p[d] < cell.max[d]  for every axis d
// -- min face inclusive, max face EXCLUSIVE. An interior seam falls to the UPPER cell; a point
// on an outer max face is OUTSIDE (returns -1, i.e. it left the world). This is exactly the
// rule the federate's ownerOf() applies, so the controller and the federates always pick the
// same owner for a given point.
int cellOf(const std::vector<Sector>& cells, const Vec3& p);

// Assign every scenario entity to a cell index by its initial position. Returns
// entityId -> cell index (std::map => iteration is id-sorted = deterministic). Throws
// std::runtime_error if any entity starts outside every cell (including exactly on an outer
// max face) -- a loud config error, never a silently unowned aircraft.
std::map<EntityId, int> assignEntities(const Scenario& scn, const std::vector<Sector>& cells);
