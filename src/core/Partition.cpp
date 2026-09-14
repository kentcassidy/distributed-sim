#include "Partition.hpp"

#include <cstddef>     // std::size_t
#include <sstream>
#include <stdexcept>

namespace {

// Vec3 has named fields, not operator[]; read/write one component by axis index (0=X,1=Y,2=Z).
double comp(const Vec3& v, int a) { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }
void   setComp(Vec3& v, int a, double val) { if (a == 0) v.x = val; else if (a == 1) v.y = val; else v.z = val; }

double volumeOf(const Sector& s) {
    return (s.max.x - s.min.x) * (s.max.y - s.min.y) * (s.max.z - s.min.z);
}

// Longest axis of a cell; ties broken X -> Y -> Z, so the choice is deterministic.
int longestAxis(const Sector& s) {
    const double ex = s.max.x - s.min.x, ey = s.max.y - s.min.y, ez = s.max.z - s.min.z;
    if (ex >= ey && ex >= ez) return 0;   // X (wins ties)
    if (ey >= ez)             return 1;    // then Y
    return 2;                              // else Z
}

// Uniform half-open membership: min inclusive, max EXCLUSIVE, on every axis. This is the
// "which cell owns this point" test -- disjoint across the RCB tiling (a seam point falls to
// the upper cell), and a point on an outer max face is outside every cell.
bool contains(const Sector& s, const Vec3& p) {
    return p.x >= s.min.x && p.x < s.max.x
        && p.y >= s.min.y && p.y < s.max.y
        && p.z >= s.min.z && p.z < s.max.z;
}

}  // namespace

std::vector<Sector> tileVolume(const Vec3& worldMin, const Vec3& worldMax, unsigned int k) {
    std::vector<Sector> cells;
    if (k == 0) return cells;

    Sector world;
    world.min = worldMin;
    world.max = worldMax;
    cells.push_back(world);

    // RCB: until we have k cells, split the largest-volume cell (ties -> lowest index) at the
    // geometric midpoint of its longest axis. Split IN PLACE -- the low half replaces the
    // parent's slot, the high half is inserted right after -- so the final ordering is
    // deterministic and spatially coherent (adjacent ids tend to be neighbours).
    while (cells.size() < k) {
        std::size_t best = 0;
        double bestVol = volumeOf(cells[0]);
        for (std::size_t i = 1; i < cells.size(); ++i) {
            const double v = volumeOf(cells[i]);
            if (v > bestVol) { bestVol = v; best = i; }   // strict > => lowest index wins ties
        }

        const Sector cell = cells[best];
        const int    axis = longestAxis(cell);
        const double mid  = 0.5 * (comp(cell.min, axis) + comp(cell.max, axis));

        Sector low = cell, high = cell;
        setComp(low.max,  axis, mid);   // low  = [min, mid) on the split axis
        setComp(high.min, axis, mid);   // high = [mid, max) -- the shared face is exactly mid

        cells[best] = low;
        cells.insert(cells.begin() + static_cast<std::ptrdiff_t>(best) + 1, high);
    }

    // Ids in final order (0..k-1) -- this is the order the controller maps onto the roster.
    for (std::size_t i = 0; i < cells.size(); ++i) cells[i].id = static_cast<unsigned int>(i);
    return cells;
}

int cellOf(const std::vector<Sector>& cells, const Vec3& p) {
    for (std::size_t i = 0; i < cells.size(); ++i)
        if (contains(cells[i], p)) return static_cast<int>(i);
    return -1;   // in no cell => outside the world
}

// SPAWN-TIME guard: validate hand-authored / generated ICs against the tiling and fail loud
// rather than silently leaving an aircraft unowned. (The RUNTIME "left the world" check -- for
// a plane that flies out mid-sim -- lives with the federate step / handoff code, not here.)
std::map<EntityId, int> assignEntities(const Scenario& scn, const std::vector<Sector>& cells) {
    std::map<EntityId, int> assignment;
    for (const EntitySpec& e : scn.entities) {
        const int idx = cellOf(cells, e.initial.position);
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
