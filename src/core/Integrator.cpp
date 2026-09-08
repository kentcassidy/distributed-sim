#include "Integrator.hpp"
#include "DynamicsModel.hpp"
#include "AircraftParams.hpp"

namespace integrator {

// TODO(M2-3): real RK4 -- four derivative evaluations, weighted combine, then
// renormalize the attitude quaternion. Stub is a no-op step (state unchanged).
State rk4Step(const State& x, double dt, const DynamicsModel& model, const AircraftParams& p) {
    return x;   // TODO
}

// TODO(M2-3): x + dt * model.derivative(x, p), attitude via quaternion kinematics.
State eulerStep(const State& x, double dt, const DynamicsModel& model, const AircraftParams& p) {
    return x;   // TODO
}

}  // namespace integrator
