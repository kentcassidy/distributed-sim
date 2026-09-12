#include "Partition.hpp"

#include <cmath>       // std::floor
#include <sstream>
#include <stdexcept>

namespace {

// Read/write one component of a Vec3 by axis. Vec3 has named fields, not operator[],
// so these keep the axis-generic code readable (and make X/Z splitting a parameter).
double comp(const Vec3& v, Axis a) {
    switch (a) {
        case Axis::X: return v.x;
        case Axis::Y: return v.y;
        case Axis::Z: return v.z;
    }
    return 0.0;   // unreachable; silences -Wreturn-type
}
void setComp(Vec3& v, Axis a, double val) {
    switch (a) {
        case Axis::X: v.x = val; break;
        case Axis::Y: v.y = val; break;
        case Axis::Z: v.z = val; break;
    }
}

// Uniform half-open world membership: min face inclusive, max face EXCLUSIVE, on every
// axis, no special cases (see Partition.hpp). This is the "is it inside the cuboid" test.
bool inWorld(const Vec3& mn, const Vec3& mx, const Vec3& p) {
    return p.x >= mn.x && p.x < mx.x
        && p.y >= mn.y && p.y < mx.y
        && p.z >= mn.z && p.z < mx.z;
}

}  // namespace

std::vector<Sector> tileVolume(const Vec3& worldMin, const Vec3& worldMax,
                               Axis axis, unsigned int k) {
    std::vector<Sector> sectors;
    if (k == 0) return sectors;

    const double lo = comp(worldMin, axis);
    const double hi = comp(worldMax, axis);
    const double width = (hi - lo) / static_cast<double>(k);

    for (unsigned int i = 0; i < k; ++i) {
        Sector s;
        s.id  = i;
        s.min = worldMin;
        s.max = worldMax;
        setComp(s.min, axis, lo + static_cast<double>(i) * width);
        // Pin the last slab's top to hi exactly so the drawn geometry meets worldMax with
        // no float drift. COSMETIC only -- ownership is slabOf()'s arithmetic, which has
        // no last-slab special case.
        setComp(s.max, axis, (i + 1 == k) ? hi : lo + static_cast<double>(i + 1) * width);
        sectors.push_back(s);
    }
    return sectors;
}

int slabOf(const Vec3& worldMin, const Vec3& worldMax, Axis axis, unsigned int k,
           const Vec3& p) {
    if (k == 0) return -1;
    if (!inWorld(worldMin, worldMax, p)) return -1;   // outside (incl. exactly on a max face)

    const double lo = comp(worldMin, axis);
    const double hi = comp(worldMax, axis);
    const double width = (hi - lo) / static_cast<double>(k);

    int idx = static_cast<int>(std::floor((comp(p, axis) - lo) / width));
    // inWorld() already guarantees lo <= p[axis] < hi, so idx is in [0, k-1]; clamp only
    // as float-rounding insurance, NOT as a semantic edge rule.
    if (idx < 0) idx = 0;
    if (idx >= static_cast<int>(k)) idx = static_cast<int>(k) - 1;
    return idx;
}

// SPAWN-TIME guard: validate hand-authored ICs against the cuboid and fail loud rather
// than silently leaving an aircraft unowned. (When procedural generation of large fleets
// arrives, it should constrain spawns to the bounds up front, so this stays a safety net,
// not the primary mechanism.) The RUNTIME "<id> lost" check -- for a plane that flies out
// mid-sim, then gets deactivated -- lives with the federate step / handoff code, not here.
std::map<EntityId, int> assignEntities(const Scenario& scn,
                                       const Vec3& worldMin, const Vec3& worldMax,
                                       Axis axis, unsigned int k) {
    std::map<EntityId, int> assignment;
    for (const EntitySpec& e : scn.entities) {
        const int idx = slabOf(worldMin, worldMax, axis, k, e.initial.position);
        if (idx < 0) {
            const Vec3& p = e.initial.position;
            std::ostringstream msg;
            msg << "Partition: entity " << e.id
                << " starts outside the world volume at ("
                << p.x << ", " << p.y << ", " << p.z << ") -- "
                << "check the scenario ICs against the world bounds";
            throw std::runtime_error(msg.str());
        }
        assignment[e.id] = idx;
    }
    return assignment;
}
