#pragma once
#include "State.hpp"
#include "AircraftParams.hpp"

// DynamicsModel (abstract) -- the one interface worth inheriting (ADR-0016 shallow
// interface). The pluggable equations of motion: given a state and the aircraft's
// params, return the state's time-derivative. RTI-free, clock-free, nothing else.
//
// PURE on purpose: no data, no wind, no gravity baked in. Wind is a post-MVP
// disturbance INPUT consumed inside the concrete model (see WindField.hpp), not a
// member of this interface; gravity rides in the concrete model (the A-matrix, for
// LinearLongitudinal).
//
// Collaborators:  Integrator calls derivative() (4x per RK4 step);
//                 LinearLongitudinal is the concrete implementation (M2);
//                 Aircraft holds a borrowed DynamicsModel* and passes it to the Integrator.
class DynamicsModel {
public:
    virtual ~DynamicsModel() = default;
    virtual StateDot derivative(const State& x, const AircraftParams& p) const = 0;
};
