#pragma once
#include "DynamicsModel.hpp"

// LinearLongitudinal : DynamicsModel -- the concrete flight model we will use (M2).
// Linearized longitudinal dynamics: a 4-state perturbation [u, w, q, theta] about
// trim with CONSTANT stability derivatives (an A-matrix). Its eigenvalues are the
// published short-period and phugoid modes, so verification is closed-form (M2-6).
//
// Collaborators:  implements DynamicsModel::derivative;
//                 reads AircraftParams (derivative set + trim);
//                 internally maps full State <-> reduced [u,w,q,theta] (M2-4);
//                 verified by the mode-check test (M2-6).
class LinearLongitudinal : public DynamicsModel {
public:
    StateDot derivative(const State& x, const AircraftParams& p) const override;
};
