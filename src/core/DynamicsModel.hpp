#pragma once
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

enum modelType {
    RK4,
    LL
}
// Not sure how to make this enum work with external resources i.e. modelType function input.

template <typename F>
struct TemplatedVecField {
    // Arbitrary function maps input position to output vector
    // Using this because std::function becomes generic at runtime by allocating heap memory. This way is much more effective... But wait! The user can't select model at runtime....
    F fieldEquation;
    // I'm still trying to understand template types. I guess the compiler prepares for it to be anything at all (as long as its variation is known at compile time)
    // then enforces typing through this bare shell function?
    Vec3 evaluate(const Vector3& pos) const {
        return fieldEquation(pos); 
    }
}
// Helper function to easily deduce types without verbose syntax. Apparently good for C++11
template <typename F>
TemplatedVecField<F> make_vector_field(F&& equation) {
    return TemplatedVecField<F>{ std::forward<F>(equation) };
}

Vec3 simpleWind(const Vec3& pos) {

}


class DynamicsModel {
public:

    DynamicsModel(std::String fieldType, std::String modelType) {
        if(fieldType == "simple") {
            this->field.evaluateForce = simpleWind;
        } else {
            std::cout << "Field Type not recognized. Current options are:\nsimple" << std::endl;
            throw std::runtime_error{"Invalid input"};
        }
        if(modelType == "RK4" || modelType == "LL"){
            this->model = modelType;
        } else {
            std::cout << "Model Type not recognized. Current options are:\nRK4\nLL" << std::endl;
            throw std::runtime_error{"Invalid input"}
        }
    }

    VecField field = 0;
    std::String model = 0;

    virtual ~DynamicsModel() = default;
    virtual StateDot derivative(const State& x, const AircraftParams& p) const {
        
        


    }; // Was = 0... Am I supposed to get rid of it? or define when I construct the Model?
};

