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

    // reference geometry (to dimensionalize aero)
    double wingArea = 0.0; // S,    m^2
    double chord    = 0.0; // cbar, m

    // trim / operating point the linear model perturbs about
    double trimSpeed = 0.0; // V0,     m/s
    double trimAlpha = 0.0; // alpha0, rad

    // TODO(M2-2): the longitudinal stability-derivative set (Xu, Xw, Zu, Zw, Mu,
    // Mw, Mq, ...) from a published/textbook source (ADR-0009). Shape TBD.
};
