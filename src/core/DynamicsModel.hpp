#pragma once
#include<functional>
#include "State.hpp"
#include "AircraftParams.hpp"
#include "Math.hpp"

// DynamicsModel (abstract) -- the one interface worth inheriting (ADR-0016 shallow
// interface). The pluggable equations of motion: given a state and the aircraft's
// params, return the state's time-derivative. RTI-free, clock-free, nothing else.
//
// Collaborators:  Integrator calls derivative() (4x per RK4 step);
//                 LinearLongitudinal is the concrete implementation (M2);
//                 Aircraft holds a borrowed DynamicsModel* and passes it to the Integrator.

struct VecField {
    std::function<Vec3(const Vec3&)> evaluateF;
}
Vec3 gravity(double mass) {
    return Vec3 {0.0, 0.0, -(mass * 9.8)}
}

Vec3 simpleTornado(const Vec3& pos) {
    // Based of this stackexchange: https://math.stackexchange.com/questions/2873752/is-there-a-tornado-ish-equation-or-vector-3d
    // Question for Claude: should we make some Matrix Mult library or nah for this scope?
    // By the way, this is the first implicit declaration that x-y is horizontal plane and z is vertical
    // Distance from center column
    double radius = std::sqrt(pos.x*pos.x + pos.y*pos.y);

    // Prevent divide by zero at exact center
    if (radius < 0.001) {
        return Vec3 { 0.0, 0.0, 1.0 }; // Straight up, assuming 1 is full magnitude
    }

    // Adjustable settings to shape tornado... Make input?? or keep as is for demo. Let's call it a stretch goal
    // Guessing good starting numbers based on plane mass of 1.
    double spinStrength           = 15.0;
    double suctionStrength        = 8.0;
    double liftStrength           = 12.0;
    double c1                     = 4.0; // Coefficients
    ////////////////////
    // Apply component vectors
    //////////
    // Rotational, I think RHR
    double spinX = -pos.y / radius * spinStrength;
    double spinY =  pos.x / radius * spinStrength;
    // Inward
    double pullX = -pos.x / radius * suctionStrength / pos.z; // Diminishes as it goes higher
    double pullY = -pos.y / radius * suctionStrength / pos.z; // Diminishes as it goes higher
    // Upward
    double liftZ = liftStrength / std::sqrt(pos.z*pos.z + radius*radius); // Diminishes by height and/or radius... Does this lead to anything? Can this be an exponential decay? or change later...?
    liftZ = (liftZ < 0.001) ? 0.0 : liftZ;
    // OH WAIT! if spin doesn't degrade, then this relationship between lift and pull can be directly modeled as an ellipse. Please help me to do that. Or a hyperbola but simple is better.

    return Vec3 {spinX + pullX, spinY + pullY, liftZ}; // My IDE isn't syntax-highlighting Vec3 as a recognized class. Please double check my syntax.
}

class DynamicsModel {
public:

    DynamicsModel(std::String fieldType = "tornado") {
        if(fieldType == "tornado") {
            this->field.evaluateF = simpleTornado;
        } else {
            std::cout << "Field Type not recognized. Current options are:\nsimple" << std::endl;
            throw std::runtime_error{"Invalid input"};
        }
    }

    VecField field = 0;
    std::String model = 0;

    virtual ~DynamicsModel() = default;
    virtual StateDot derivative(const State& x, const AircraftParams& p) const {
        Vec3 field_at_pos = this->evaluateF(x.position) + gravity(p.mass);

        // Apply field_at_pos to do flight math
        // BIG TODO.
        // Shoot, I see it was supposed to be implemented in the LinearLongitudinal.cpp. Is this also for my functions above?
    };
};

