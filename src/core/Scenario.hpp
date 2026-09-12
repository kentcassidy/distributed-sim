#pragma once
#include <string>
#include <vector>
#include "AircraftParams.hpp"   // EntityId
#include "State.hpp"

// Scenario -- the SHARED description of a run: which entities exist and where each
// one starts. It is the single source of truth for a scenario, read ONCE by the
// controller, which then disseminates the per-entity initial conditions to whichever
// federate it assigns that entity to (over the RTI -- ADR-0015). The SAME scenario
// drives every partitioning: K=1 (all entities co-located on one federate) and K=2
// (one each) run the identical entities from the identical ICs; only the controller's
// id -> federate assignment differs. That is exactly what makes partition invariance
// checkable -- so the scenario is deliberately kept OUT of any federate and OUT of the
// RTI transport, a plain deterministic data file.
//
// SCOPE: only identity + initial conditions vary per entity for the MVP. The physics
// PARAMS (mass, aero derivatives) are the shared filler defaults (AircraftParams), so
// the scenario carries just the id and the starting State. A per-entity params column
// is a later, backward-compatible addition.

// One entity as declared by the scenario: a stable federation-wide id + its State at
// t = 0 (position, velocity, attitude, angular velocity).
struct EntitySpec {
    EntityId id = 0;
    State    initial;
};

// The whole scenario: an id-sorted list of entities. Determinism starts HERE -- the
// list is sorted by id at load time so downstream iteration order never depends on
// the order lines happened to appear in the file.
struct Scenario {
    std::vector<EntitySpec> entities;

    // Find one entity's spec by id; nullptr if this scenario has no such id. (Small N,
    // so a linear scan is fine and keeps the type dependency-free.)
    const EntitySpec* find(EntityId id) const;
};

// Load a scenario from a CSV file. One entity per line, comma-separated; blank lines
// and '#' / ';' comment lines (e.g. the header row) ignored:
//
//     # id, x, y,    z, vx,  vy, vz, pitch
//     1,    0, 500,  0, 200, 0,  0,  0.02
//     2,    0, 1000, 0, 200, 0,  0,  0.02
//
// `pitch` is a convenience: a pure pitch about +y, stored as the exact half-angle
// quaternion (0, sin(p/2), 0, cos(p/2)) so the initial attitude is already a unit
// quaternion. angularV starts at zero.
//
// Throws std::runtime_error on a missing file, a malformed line, or a duplicate id --
// a scenario typo must fail LOUDLY at controller startup, never silently drop an
// entity (which would later look like a partition-invariance bug rather than a config
// error).
Scenario loadScenario(const std::string& path);
