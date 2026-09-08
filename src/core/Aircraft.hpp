#pragma once
#include "State.hpp"
#include "AircraftParams.hpp"

class DynamicsModel;   // fwd (borrowed, not owned)

// Aircraft (State, AircraftParams, DynamicsModel*) -- one simulated entity.
// advance(dt) integrates ITSELF. HAS-A, not IS-A (composition -- ADR-0016).
//
// Ownership:  World owns Aircraft BY VALUE; the DynamicsModel is BORROWED (raw,
//             non-owning ptr) -- models are owned elsewhere (a registry/federate)
//             and shared across aircraft of the same archetype.
// Collaborators:  World::advance calls advance();
//                 advance() calls integrator::rk4Step with model_;
//                 Collision mutates state() (a bounce) and reads params() (mass/radius);
//                 the federate reads state() to publish and to test sector membership.
class Aircraft {
public:
    Aircraft(EntityId id, const AircraftParams& params, const DynamicsModel* model);

    void advance(double dt);                       // one integration step (M2-5)

    EntityId              id()     const { return id_; }
    const State&          state()  const { return state_; }
    State&                state()        { return state_; }   // Collision mutates this
    const AircraftParams& params() const { return params_; }

private:
    EntityId             id_;
    State                state_;
    AircraftParams       params_;
    const DynamicsModel* model_;   // borrowed; not owned
};
