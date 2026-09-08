#include "Integrator.hpp"
#include "DynamicsModel.hpp"
#include "AircraftParams.hpp"

namespace integrator {

// Helper function to renormalize the orientation quaternion inside State
void renormAtt(State& state) {
    double length = std::sqrt(state.attitude.x*state.attitude.x
                            + state.attitude.y*state.attitude.y
                            + state.attitude.z*state.attitude.z
                            + state.attitude.w*state.attitude.w);
    if (length > 0.00001) {
        state.attitude.x /= length;
        state.attitude.y /= length;
        state.attitude.z /= length;
        state.attitude.w /= length;
    }
}

// Next: s + dt * model.derivative(s, p), attitude via quaternion kinematics.
State eulerStep(const State& s, double dt, const DynamicsModel& model, const AircraftParams& p) {
    StateDot ds = model.derivative(s, p);
    State s_next = s + ds * dt;
    
    renormAtt(s_next);
    
    return s_next;
}

// Next: real RK4 -- four derivative evaluations, weighted combine, then
// renormalize the attitude quaternion. Stub is a no-op step (state unchanged).
State rk4Step(const State& s, double dt, const DynamicsModel& model, const AircraftParams& p) {
    double half_dt = dt * 0.5;

    StateDot k1 = model.derivative(s, p);

    State s_k2 = s + k1 * half_dt;
    renormAtt(s_k2);
    StateDot k2 = model.derivative(s_k2, p);

    State s_k3 = s + k2 * half_dt;
    renormAtt(s_k3);
    StateDot k3 = model.derivative(s_k3, p);

    State s_k4 = s + k3 * half_dt;
    renormAtt(s_k4);
    StateDot k4 = model.derivative(s_k4, p);

    // Simpson's Rule weights
    StateDot final_derivative = (k1 + k2 * 2.0 + k3 * 2.0 + k4) * (1.0 / 6.0);

    State s_next = s + final_derivative * dt;
    renormAtt(s_next)

    return s_next;
}

}  // namespace integrator
