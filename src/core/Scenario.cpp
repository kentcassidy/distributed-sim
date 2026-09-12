#include "Scenario.hpp"

#include <algorithm>   // std::sort, std::adjacent_find
#include <cmath>       // std::sin, std::cos
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

// find() -- linear scan; N is tiny (two aircraft for the MVP). Returns a pointer INTO
// entities, so it is only valid while the Scenario is alive and unmodified.
const EntitySpec* Scenario::find(EntityId id) const {
    for (const EntitySpec& e : entities) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

namespace {

// Build a unit quaternion for a pure pitch (rotation about +y) of `pitch` radians.
// Half-angle form so the result is already normalized: q = (0, sin(p/2), 0, cos(p/2)).
// This matches how LinearLongitudinal reads pitch back out (theta ~= 2*attitude.y).
Quat pitchQuat(double pitch) {
    double half = 0.5 * pitch;
    return Quat(0.0, std::sin(half), 0.0, std::cos(half));
}

// Is this a line we skip entirely -- empty, all-whitespace, or a comment?
// Comments start with '#' or ';' (';' matches the .fed convention already in the repo).
bool isSkippable(const std::string& line) {
    for (char c : line) {
        if (c == '#' || c == ';') return true;         // first non-space is a comment marker
        if (!std::isspace(static_cast<unsigned char>(c))) return false;
    }
    return true;                                        // reached end with only whitespace
}

}  // namespace

Scenario loadScenario(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Scenario: cannot open file '" + path + "'");
    }

    Scenario scn;
    std::string line;
    unsigned int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        if (isSkippable(line)) continue;

        // CSV fields, comma-separated. Turn commas into spaces and reuse the stream
        // extractor, so any spacing works ("1,0,500" or "1, 0, 500"). Expect exactly
        // 8 fields: id,x,y,z,vx,vy,vz,pitch.
        for (char& ch : line) { if (ch == ',') ch = ' '; }

        std::istringstream ss(line);
        EntitySpec e;
        double px, py, pz, vx, vy, vz, pitch;
        if (!(ss >> e.id >> px >> py >> pz >> vx >> vy >> vz >> pitch)) {
            throw std::runtime_error(
                "Scenario: malformed line " + std::to_string(lineNo) + " in '" + path +
                "' (expected: id,x,y,z,vx,vy,vz,pitch)");
        }
        // Reject trailing junk so a stray extra column is caught, not ignored.
        std::string extra;
        if (ss >> extra) {
            throw std::runtime_error(
                "Scenario: extra data on line " + std::to_string(lineNo) + " in '" + path +
                "' (unexpected '" + extra + "')");
        }

        e.initial.position = Vec3(px, py, pz);
        e.initial.velocity = Vec3(vx, vy, vz);
        e.initial.attitude = pitchQuat(pitch);
        e.initial.angularV = Vec3(0.0, 0.0, 0.0);
        scn.entities.push_back(e);
    }

    // Determinism: sort by id so iteration order is fixed regardless of file order.
    std::sort(scn.entities.begin(), scn.entities.end(),
              [](const EntitySpec& a, const EntitySpec& b) { return a.id < b.id; });

    // A duplicate id is ambiguous (two ICs for one entity) -- fail loudly. After the
    // sort, duplicates are adjacent.
    auto dup = std::adjacent_find(
        scn.entities.begin(), scn.entities.end(),
        [](const EntitySpec& a, const EntitySpec& b) { return a.id == b.id; });
    if (dup != scn.entities.end()) {
        throw std::runtime_error(
            "Scenario: duplicate entity id " + std::to_string(dup->id) + " in '" + path + "'");
    }

    if (scn.entities.empty()) {
        throw std::runtime_error("Scenario: '" + path + "' declares no entities");
    }

    return scn;
}
