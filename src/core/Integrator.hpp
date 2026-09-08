#pragma once
#include "State.hpp"

class DynamicsModel;    // fwd
struct AircraftParams;  // fwd

// Integrator -- RK4; free functions, no state to carry (as noted, that's fine).
// PURE: same inputs -> same output, which is what keeps runs reproducible, and
// reproducibility is what partition invariance is checked against.
//
// Collaborators:  called by Aircraft::advance;
//                 calls DynamicsModel::derivative (4x for rk4, 1x for euler).
//
// NOTE: State.attitude is a quaternion -- it does NOT integrate by plain addition.
// A step advances position/velocity/omega linearly but advances attitude by
// quaternion kinematics and RENORMALIZES. Flagged here; handled in the .cpp (M2-3).
namespace integrator {
    State rk4Step  (const State& s, double dt, const DynamicsModel& model, const AircraftParams& p);
    State eulerStep(const State& s, double dt, const DynamicsModel& model, const AircraftParams& p);  // interchangeable simplest (ADR-0016)
}
