#pragma once
#include <functional>
#include <cmath>
#include "Math.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// WindField -- PARKED, POST-MVP. The storm / turbulence is CUT from the MVP
// (charter Scope; ADR-0010), so nothing includes this yet -- it is the ready-to-use
// home for the field when the storm returns.
//
// HOW WIND WILL ENTER THE MODEL (charter's "one point" rule):
//     airspeed = inertial velocity - wind(position)
// sampled at the aircraft's position and subtracted BEFORE the aerodynamics. So
// wind is a DISTURBANCE INPUT consumed inside the concrete model's derivative(),
// never a member of the DynamicsModel interface. In state-space terms it is the
// E*w disturbance channel -- distinct from A (plant), B (control), C (output).
// ─────────────────────────────────────────────────────────────────────────────

// A sampled vector field: give it a position, get the wind vector there.
struct VecField {
    std::function<Vec3(const Vec3&)> evaluateF;
};

// Gravity as a body force (mass * g, down). NOTE: for the LINEARIZED longitudinal
// model gravity is already inside the A-matrix (the -g*cos(th0) / -g*sin(th0)
// terms); this free function is for a future nonlinear/6-DOF model that sums forces.
inline Vec3 gravity(double mass) {
    return Vec3{ 0.0, 0.0, -(mass * 9.81) };
}

// A synthetic tornado, built by SUMMING three component fields -- the standard way
// to author a vortex: tangential spin + inward suction + vertical lift.
// Source: https://math.stackexchange.com/questions/2873752/is-there-a-tornado-ish-equation-or-vector-3d
// Convention (first stated here): x-y is the horizontal plane, z is vertical / up.
// The falloffs are HEURISTIC -- to be tuned; the field is NOT divergence-free (the
// suction is a sink), which is fine for a forcing field but not a physical wind.
inline Vec3 simpleTornado(const Vec3& pos) {
    // distance from the center column
    double radius = std::sqrt(pos.x*pos.x + pos.y*pos.y);

    // Prevent divide by zero at exact center
    if (radius < 0.001) {
        return Vec3{ 0.0, 0.0, 1.0 };   // Straight up, assuming 1 is full magnitude
    }

    // Adjustable settings to shape tornado. Make input?? or keep for demo (stretch).
    // Guessing good starting numbers based on plane mass of 1.
    double spinStrength    = 15.0;
    double suctionStrength = 8.0;
    double liftStrength    = 12.0;

    // Guard height so the /z falloffs don't blow up (or invert) near/below ground.
    double height = (pos.z > 0.1) ? pos.z : 0.1;

    ////////////////////
    // Apply component vectors
    //////////
    // Rotational (tangential, CCW about +z by RHR)
    double spinX = -pos.y / radius * spinStrength;
    double spinY =  pos.x / radius * spinStrength;
    // Inward, diminishes as it goes higher
    double pullX = -pos.x / radius * suctionStrength / height;
    double pullY = -pos.y / radius * suctionStrength / height;
    // Upward, diminishes by height and/or radius
    double liftZ = liftStrength / std::sqrt(height*height + radius*radius);
    liftZ = (liftZ < 0.001) ? 0.0 : liftZ;
    // OH WAIT! if spin doesn't degrade, the lift<->pull relationship can be modeled
    // as an ellipse (or a hyperbola -- but simple is better). Revisit in tuning.

    return Vec3{ spinX + pullX, spinY + pullY, liftZ };
}

// ── Design-trail notes carried from drafting (kept for the GH history) ───────────
// Curl(f) = del-cross-f (the determinant with i,j,k / d/dx.. / f_1..). Refresher:
//   curl(vector)->vector, grad(scalar)->vector, div(vector)->scalar, laplacian: scalar->scalar.
// A physical incompressible wind is divergence-free (no source/sink); a 2D slice of
// a high/low can look similar. Do we even need to model curl? No -- we map the field
// and "apply" the sampled vector as forcing (no del-cross, no CFD: "aero is a table,
// not a solve"). That also means we can stay in 3D vectors sampled pointwise -- no
// need to translate the field or carry a big transform. That instinct was right.
