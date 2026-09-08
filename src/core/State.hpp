#pragma once
#include "Math.hpp"

struct State {
    Vec3 position;
    Vec3 velocity;
    Quat attitude;
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
// Why create a whole new struct just for the derivative? Isn't this in name only? I guess that's fine but sort of makes me need to create unique operators 
// Why not the following:
//using StateDot = State;
// Lesson Learned... Need to enforce compiler safety for physics bugs lest this gets confusing.

// Returns a temporary StateDot where every rate is multiplied by dt, effectively integrated.
inline StateDot operator*(const StateDot& dot, double dt) {
    return StateDot{
        dot.dPosition * dt,
        dot.dVelocity * dt,
        dot.dAttitude * dt,
        dot.dAngularV * dt
    };
}
// Retain commutativity by reusing above
inline StateDot operator*(double dt, const StateDot& dot) {
    return dot * dt;
}

inline State operator+(const State& s, const StateDot& dot) {
    return State {
        s.position + dot.dPosition,
        s.velocity + dot.dVelocity,
        s.attitude + dot.dAttitude,
        s.angularV + dot.dAngularV
    };
}

// StateDot + StateDot -- needed to combine the RK4 stages: (k1 + 2*k2 + 2*k3 + k4).
inline StateDot operator+(const StateDot& a, const StateDot& b) {
    return StateDot {
        a.dPosition + b.dPosition,
        a.dVelocity + b.dVelocity,
        a.dAttitude + b.dAttitude,
        a.dAngularV + b.dAngularV
    };
}

// (Curl / divergence wind notes moved to WindField.hpp -- their proper home.)

