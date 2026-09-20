// ─────────────────────────────────────────────────────────────────────────────
// gen_scenario -- seeded random scenario generator.
//
//     gen_scenario [--seed N] [--count N] [--out PATH] [--speed-min V] [--speed-max V]
//
// Writes a deterministic IC CSV the controller loads: `id, x, y, z, vx, vy, vz, pitch`
// (the exact shape Scenario.cpp expects). Standalone C++17 -- no RTI, no dff_core, links
// nothing (mirrors dff_diff).
//
// The CSV is the reproducible ARTIFACT: the same file, read once by the controller and
// disseminated to every federate, gives identical ICs to K=1 and K=2 -- so partition
// invariance stays checkable. (The generator's own RNG output need not match across
// machines; the committed/produced CSV is what matters, and it round-trips exactly at
// setprecision(17).)
//
// Aircraft are placed UNIFORMLY inside the world (a small inset keeps them off the
// half-open max faces, which the controller's cellOf() rejects), each with a velocity in a
// uniformly-random 3D direction at a random speed. So they cross sector seams in every
// direction AND some fly clear out of the world -- exercising both the peer handoff and the
// "lost in the void" path in one run.
//
// NOTE: the world bounds below MUST match ControllerFederate.cpp's WORLD_MIN / WORLD_MAX.
// ─────────────────────────────────────────────────────────────────────────────
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <utility>   // std::swap

int main(int argc, char** argv) {
    // --- defaults ---
    std::uint64_t seed     = 42;
    int           count    = 24;
    std::string   out      = "scenarios/random.csv";
    double        speedMin = 120.0;
    double        speedMax = 240.0;

    // World bounds -- KEEP IN SYNC with ControllerFederate.cpp WORLD_MIN / WORLD_MAX.
    const double xmin = 0.0,    ymin = 0.0,    zmin = -500.0;
    const double xmax = 2500.0, ymax = 1500.0, zmax =  500.0;

    // --- args ---
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* what) -> std::string {
            if (i + 1 >= argc) { std::cerr << "missing value for " << what << "\n"; std::exit(2); }
            return argv[++i];
        };
        if      (a == "--seed")      seed     = std::stoull(next("--seed"));
        else if (a == "--count")     count    = std::stoi(next("--count"));
        else if (a == "--out")       out      = next("--out");
        else if (a == "--speed-min") speedMin = std::stod(next("--speed-min"));
        else if (a == "--speed-max") speedMax = std::stod(next("--speed-max"));
        else { std::cerr << "usage: gen_scenario [--seed N] [--count N] [--out PATH] "
                            "[--speed-min V] [--speed-max V]\n"; return 2; }
    }
    if (count <= 0)          { std::cerr << "count must be positive\n"; return 2; }
    if (speedMax < speedMin) std::swap(speedMin, speedMax);

    // Inset so every start is strictly interior (off the EXCLUSIVE max faces, which
    // assignEntities() treats as outside the world and rejects with a loud error).
    const double inset = 0.02;
    const double ix = (xmax - xmin) * inset;
    const double iy = (ymax - ymin) * inset;
    const double iz = (zmax - zmin) * inset;

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> ux(xmin + ix, xmax - ix);
    std::uniform_real_distribution<double> uy(ymin + iy, ymax - iy);
    std::uniform_real_distribution<double> uz(zmin + iz, zmax - iz);
    std::uniform_real_distribution<double> uspeed(speedMin, speedMax);
    std::normal_distribution<double>       gauss(0.0, 1.0);  // normalized -> uniform on the sphere

    std::ofstream f(out);
    if (!f) { std::cerr << "cannot open '" << out << "' for writing\n"; return 2; }
    f << std::setprecision(17);   // exact double round-trip -> identical ICs across partitionings

    f << "# seeded random scenario -- seed=" << seed << ", count=" << count
      << ", speed=[" << speedMin << "," << speedMax << "]\n";
    f << "# world x[" << xmin << "," << xmax << ") y[" << ymin << "," << ymax
      << ") z[" << zmin << "," << zmax << ")  -- MUST match the controller's WORLD_MIN/MAX\n";
    f << "# id, x, y, z, vx, vy, vz, pitch\n";

    for (int id = 1; id <= count; ++id) {
        const double x = ux(rng), y = uy(rng), z = uz(rng);

        // Uniform direction on the unit sphere = a normalized 3D Gaussian; scale by speed.
        double dx = gauss(rng), dy = gauss(rng), dz = gauss(rng);
        double n  = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (n < 1e-12) { dx = 1.0; dy = 0.0; dz = 0.0; n = 1.0; }  // degenerate draw guard
        const double sp = uspeed(rng);
        const double vx = sp * dx / n, vy = sp * dy / n, vz = sp * dz / n;

        // pitch = 0: initial attitude is level. Orientation is filler here (the model has no
        // heading dynamics), so this is just a clean starting attitude.
        f << id << "," << x << "," << y << "," << z << ","
          << vx << "," << vy << "," << vz << ",0\n";
    }

    std::cout << "wrote " << count << " aircraft to " << out << "  (seed " << seed
              << ", speed [" << speedMin << "," << speedMax << "])\n";
    return 0;
}
