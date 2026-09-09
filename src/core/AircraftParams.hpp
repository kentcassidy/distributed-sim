#pragma once
#include "Math.hpp"

// AircraftParams (mass, aero derivatives) -- pre-aircraft data, the config-driven
// archetype (ADR-0013 / ADR-0016). Static per-aircraft constants; never integrated.
//
// Collaborators:  DynamicsModel reads mass/inertia/geometry + the derivative set;
//                 Collision reads mass + radius for the impulse;
//                 loaded from the scenario config into each Aircraft.

// Federation-wide entity identity -- stable across runs and config. Distinct from
// the RTI's per-run ObjectInstanceHandle (which the federate marries to this id).
using EntityId = unsigned int;

struct AircraftParams {
    EntityId id = 0;

    // rigid body
    double mass   = 0.0;   // kg
    double Iyy    = 0.0;   // pitch moment of inertia, kg*m^2 (longitudinal)
    double radius = 0.0;   // collision bounding radius, m

    // trim / operating point the linear model perturbs about.
    //
    // SCOPE: this flight model is deliberately MINIMAL FILLER for a distributed-
    // systems proof of concept. It only has to be (1) stable/bounded and (2)
    // deterministic, so there is *something* to distribute and check for partition
    // invariance (identical result co-located vs split across machines -- the real
    // V&V). The numbers below are ARBITRARY, not a real aircraft; fidelity is out
    // of scope on purpose. Units are nominally SI but immaterial to the PoC.
    double trimSpeed = 200.0;  // u0,     m/s    steady cruise -- carries the aircraft across sectors
    double trimPitch = 0.0;    // theta0, rad
    double trimW     = 0.0;    // w0,     m/s    trim vertical velocity (~0)
    double gravity   = 9.81;   // g,      m/s^2

    // "Longitudinal stability derivatives": arbitrary values chosen so the 4x4
    // A-matrix is stable (eigenvalues with negative real parts) -> damped, bounded
    // motion that never diverges. LinearLongitudinal folds in the Mw_dot corrections.
    struct LonDerivs {
        double Xu = -0.02, Xw =  0.02, Xq = 0.0;
        double Zu = -0.20, Zw = -0.50, Zq = 0.0;
        double Mu =  0.00, Mw = -0.02, Mq = -0.50, Mw_dot = 0.0;
    } lon;
};
