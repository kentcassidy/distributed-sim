#pragma once
#include "State.hpp"
#include "AircraftParams.hpp"

// DynamicsModel (abstract) -- the one interface worth inheriting (ADR-0016 shallow
// interface). The pluggable equations of motion: given a state and the aircraft's
// params, return the state's time-derivative. RTI-free, clock-free, nothing else.
//
// Collaborators:  Integrator calls derivative() (4x per RK4 step);
//                 LinearLongitudinal is the concrete implementation (M2);
//                 Aircraft holds a borrowed DynamicsModel* and passes it to the Integrator.
class DynamicsModel {
public:
    virtual ~DynamicsModel() = default;
    virtual StateDot derivative(const State& x, const AircraftParams& p) const = 0;
};
