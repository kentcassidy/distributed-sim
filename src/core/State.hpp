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



// Curl(f) --> cross-product of ∇(nabla?) and f:
/*
        |   i       j       k   |
= det   |   d/dx    d/dy    d/dz|
        |   f_1     f_2     f_3 |

=

Curl takes in vector field and outputs a vector
Grad takes in a scalar and returns a vector
Div takes in a vector and returns a scalar

Del squared (Laplacian?) is Scalar -> Scalar

Note that in 3D, wind should not have a source or sink == total divergence of zero. High and low may be similar from a 2D perspective at a slice.

Well actually, do we even need to model the actual curl? I think we can map out the vector field (rotational angular transformation matrix A)
and any step can just "apply" a force vector to the aircraft that does its calculation??

However this should also mean we can stick with 3D matrices as we don't rely on translation of the vector field (assuming no divergence??). Am I correct?

*/

