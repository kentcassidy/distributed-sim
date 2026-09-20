#pragma once
#include "DynamicsModel.hpp"

// WeathervaneFlight : DynamicsModel -- the ROTATIONAL half of a 6-DOF flight model, added so an
// aircraft actually turns its nose toward where it is going. LinearLongitudinal has no yaw or
// roll at all, so a plane with lateral velocity never reorients; this model gives it real
// attitude dynamics: the nose WEATHERVANES toward the velocity vector (pitch + yaw restoring),
// banking briefly as it settles. Velocity is HELD (Option A) -- a force model (lift/thrust/
// gravity) that curves the velocity into coordinated turns is a deliberate follow-up (Option B).
//
// Still deterministic FILLER (arbitrary-but-stable gains in AircraftParams::AeroFiller): its
// only jobs are to be stable/bounded and deterministic, so partition invariance stays bit-exact.
// It weathervanes into a wind gust for free once a wind field feeds relative velocity in.
//
// Conventions: velocity is world-frame; attitude q rotates body->world; angularV is the
// body-frame rate (x=roll, y=pitch, z=yaw); nose is body +X; world Z is up.
class WeathervaneFlight : public DynamicsModel {
public:
    StateDot derivative(const State& x, const AircraftParams& p) const override;
};
