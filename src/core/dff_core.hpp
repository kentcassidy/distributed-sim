#pragma once
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// dff_core — public surface of the RTI-free physics core.
//
// DESIGN RULE 1 (RTI-free): nothing in this directory may #include a Portico /
// ieee1516e header. The core is the model; the RTI is a transport. Keeping them
// apart is what lets the core compile as clean C++17 and be unit-tested with no
// federation running.
//
// DESIGN RULE 2 (header standard): headers here are #included by the federate
// and controller, which compile as gnu++14. So the PUBLIC headers must stay
// C++14-safe. The .cpp bodies may use full C++17 — the standard split lives at
// the source boundary, not the header boundary.
//
// This file currently exposes only a version smoke-test, so the whole build
// graph is green end to end (a C++17 static lib linked into gnu++14
// executables). Grow it into the real surface as you build the model:
//   State (pos, quat, vel, omega) · Integrator (RK4) · Aircraft (advance) ·
//   Dynamics · Collision (elastic) · World (holds aircraft in a region, steps
//   them, resolves local collisions).
// ─────────────────────────────────────────────────────────────────────────────
namespace dff {

// Returns a human-readable version/identity string for the core. Smoke test
// only — proves the link across the C++17-lib / gnu++14-exe boundary.
std::string core_version();

}  // namespace dff
