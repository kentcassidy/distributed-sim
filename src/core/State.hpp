#pragma once
#include "Math.hpp"

struct State {
    Vec3 position;
    Vec3 velocity;
    Quat attitude;
    double mass = 0.0;
    Vec3 angularV;

    //EntityID here or elsewhere?
};