#include "Aircraft.hpp"
#include "Integrator.hpp"
#include "DynamicsModel.hpp"

Aircraft::Aircraft(EntityId id, const AircraftParams& params, const DynamicsModel* model)
    : id_(id), state_(), params_(params), model_(model) {}

// One step. The integrator is where the physics TODO lives; this just delegates,
// which is the whole point of the model/integrator split (ADR-0016).
void Aircraft::advance(double dt) {
    state_ = integrator::rk4Step(state_, dt, *model_, params_);   // TODO: real step in rk4Step
}
