#pragma once
#include "Math.hpp"

struct State {
    Vec3 position;
    Vec3 velocity;
    Quat attitude;
    double mass = 0.0;
    Vec3 angularV;

    // EntityId lives on Aircraft/AircraftParams, not here: identity is not physics,
    // and State is exactly the thing the Integrator advances.
};

// StateDot -- the time-derivative of State. Produced by DynamicsModel::derivative,
// consumed by the Integrator. Field k holds d(State.k)/dt:
//   dPosition = velocity,   dVelocity = acceleration,
//   dAttitude = quaternion RATE (0.5 * omega (x) q),   dAngularV = angular acceleration.
// (mass has no rate -- it is a parameter, not integrated.)
struct StateDot {
    Vec3 dPosition;
    Vec3 dVelocity;
    Quat dAttitude;    // a RATE, not a unit quaternion; the integrator renormalizes
    Vec3 dAngularV;
};